extern "C" {
#include "http.h"

#include <esp_http_server.h>
#include <esp_event.h>
#include <esp_log.h>

#include <stdio.h>

#include "global_context.h"
}

#include "compression/lib/compression.hpp"

#include <ff.h>

#include <string>

static const char* TAG = "http-server";

#include "http_strings.hpp"

#define HANDLE(x)        \
    if ((x) != ESP_OK) { \
        return ESP_FAIL; \
    }

static esp_err_t respond_with_file(httpd_req_t* req, const char* filename) {
    static uint8_t buf2[2000];
    static char buf_text[4096];

    gctx.pause_polling = true;

    const double sample_rate = 8000.0 / 14;
    const double gscale = 1 / 0.00053263221;

    HANDLE(httpd_resp_send_chunk(req, R"--(GYROFLOW IMU LOG
version,1.1
id,esplog
orientation,YxZ
tscale,0.00180
gscale,0.00053263221
ascale,0.0001
t,gx,gy,gz,ax,ay,ax
)--",
                          HTTPD_RESP_USE_STRLEN));

    ESP_LOGI(TAG, "Reading file");
    FILE* f = fopen(filename, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }

    fseek(f, 0L, SEEK_END);
    ESP_LOGI(TAG, "file size is %d bytes", ftell(f));
    fseek(f, 0L, SEEK_SET);

    Coder decoder(kBlockSize);

    int have_bytes = 0;
    int time = 0;
    quat::quat prev_quat{};
    char* wptr = buf_text;
    while (true) {
        int read_bytes = fread(buf2 + have_bytes, 1, 2000 - have_bytes, f);
        have_bytes += read_bytes;

        if (have_bytes < 2000) break;

        auto [decoded_bytes, dquats, scale] = decoder.decode_block(buf2);
        for (auto& q : dquats) {
            quat::vec rv = (q.conj() * prev_quat).axis_angle();
            quat::vec gravity = q.conj().rotate_point(
                {quat::base_type{}, quat::base_type{}, quat::base_type{-1.0}});

            prev_quat = q;
            if (time != 0) {
                double scale = sample_rate * gscale;
                double ascale = 10000;
                int size =
                    snprintf(wptr, sizeof(buf_text) - (wptr - buf_text), "%d,%d,%d,%d,%d,%d,%d\n",
                             time, (int)(double(rv.x) * scale), (int)(double(rv.y) * scale),
                             (int)(double(rv.z) * scale), (int)(double(gravity.x) * ascale),
                             (int)(double(gravity.y) * ascale), (int)(double(gravity.z) * ascale));
                wptr += size;
            }
            time++;

            if (wptr - buf_text > 3200) {
                if(httpd_resp_send_chunk(req, buf_text, HTTPD_RESP_USE_STRLEN) != ESP_OK) {
                    fclose(f);
                    return ESP_FAIL;
                }
                wptr = buf_text;
            }
        }

        have_bytes -= decoded_bytes;
        ESP_LOGI(TAG, "%d block uncompressed, scale = %d", decoded_bytes, scale);

        taskYIELD();
        memmove(buf2, buf2 + decoded_bytes, have_bytes);
    }
    fclose(f);

    if (wptr != buf_text) {
        HANDLE(httpd_resp_send_chunk(req, buf_text, HTTPD_RESP_USE_STRLEN));
    }

    HANDLE(httpd_resp_send_chunk(req, NULL, 0));

    gctx.continue_polling = true;

    return ESP_OK;
}

static esp_err_t download_get_handler(httpd_req_t* req) {
    char buf[128];
    int buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > sizeof(buf)) {
        return ESP_FAIL;
    }
    if (buf_len > 1 && httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
        ESP_LOGI(TAG, "Found URL query => %s", buf);
        char param[32];
        if (httpd_query_key_value(buf, "name", param, sizeof(param)) == ESP_OK) {
            ESP_LOGI(TAG, "want to download %s", param);
            snprintf(buf, sizeof(buf), "/spiflash/%s", param);
            httpd_resp_set_hdr(req, "content-disposition", "attachment;filename=gyro.gcsv");
            return respond_with_file(req, buf);
        }
    }

    return ESP_FAIL;
}

static const httpd_uri_t download_get = {
    .uri = "/download", .method = HTTP_GET, .handler = download_get_handler, NULL};

std::pair<int, int> get_free_space_kb() {
    constexpr int kSectorBytes = 4096;
    FATFS* fs;
    DWORD fre_clust, fre_sect, tot_sect;
    /* Get volume information and free clusters of drive 0 */
    FRESULT res = f_getfree("0:", &fre_clust, &fs);
    /* Get total sectors and free sectors */
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;
    return {kSectorBytes * fre_sect / 1024, kSectorBytes * tot_sect / 1024};
}

static esp_err_t root_get_handler(httpd_req_t* req) {
    HANDLE(httpd_resp_send_chunk(req, html_prefix, HTTPD_RESP_USE_STRLEN));

    HANDLE(httpd_resp_send_chunk(req, "<body><h1>EspLog (", HTTPD_RESP_USE_STRLEN));
    HANDLE(httpd_resp_send_chunk(req, gctx.logger_control.busy ? "BUSY" : "IDLE", HTTPD_RESP_USE_STRLEN));

    HANDLE(httpd_resp_send_chunk(req, R"--()</h1>
    <h2>Control</h2>
    <button style="color:green;" class="command_btn" name="command" form="form_simple" value="calibrate">0</button>
    <button style="color:red;" class="command_btn" name="command" form="form_simple" value="record">&#x23fa;</button>
    <button style="color:black;" class="command_btn" name="command" form="form_simple" value="stop">&#x23F9;</button>

    <h2>Status</h2>
    <table class="status_table">
        <tr>
            <td>Active</td>
            <td class="status_value_table_cell">)--",
                          HTTPD_RESP_USE_STRLEN));

    HANDLE(httpd_resp_send_chunk(req, std::to_string(gctx.logger_control.busy).c_str(),
                          HTTPD_RESP_USE_STRLEN));

    HANDLE(httpd_resp_send_chunk(req, R"--(</td>
        </tr>
        <tr>
            <td>Free space (kBytes)</td>
            <td class="status_value_table_cell">)--",
                          HTTPD_RESP_USE_STRLEN));

    auto free_space = get_free_space_kb();
    HANDLE(httpd_resp_send_chunk(req, std::to_string(free_space.first).c_str(), HTTPD_RESP_USE_STRLEN));

    HANDLE(httpd_resp_send_chunk(req, R"--(</td>
        </tr>
    <tr>
        <td>Last log length (samples)</td>
        <td class="status_value_table_cell">)--",
                          HTTPD_RESP_USE_STRLEN));

    HANDLE(httpd_resp_send_chunk(req, std::to_string(gctx.logger_control.total_samples_written).c_str(),
                          HTTPD_RESP_USE_STRLEN));

    HANDLE(httpd_resp_send_chunk(req, R"--(</td>
        </tr>
        <tr>
            <td>Last log avg rate (Bytes/min)</td>
            <td class="status_value_table_cell">)--",
                          HTTPD_RESP_USE_STRLEN));

    HANDLE(httpd_resp_send_chunk(req,
                          std::to_string(gctx.logger_control.avg_logging_rate_bytes_min).c_str(),
                          HTTPD_RESP_USE_STRLEN));

    HANDLE(httpd_resp_send_chunk(req, R"--(</td>
        </tr>
    </table>
    <h2>Log download</h2>
    <table class="download_table">)--",
                          HTTPD_RESP_USE_STRLEN));
    bool busy = true;
    if (xSemaphoreTake(gctx.logger_control.mutex, 2)) {
        busy = gctx.logger_control.busy;
        xSemaphoreGive(gctx.logger_control.mutex);
    }

    if (!busy) {
        for (int i = 0; i <= 100; ++i) {
            static constexpr char templ[] = "/spiflash/log%03d.bin";
            char buf[30], buf2[256];
            snprintf(buf, sizeof(buf), templ, i);
            FILE* f = fopen(buf, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                int size_kb = ftell(f) / 1024;
                fclose(f);

                snprintf(buf2, sizeof(buf2), R"--(<tr>
            <td><a href="/download?name=log%03d.bin">log%03d.bin</a></td>
            <td class="download_table_mid_cell">%dKB</td>)--",
                         i, i, size_kb);

                HANDLE(httpd_resp_send_chunk(req, buf2, HTTPD_RESP_USE_STRLEN));

                snprintf(buf2, sizeof(buf2), R"--(<td>
                <button style="color:black;" class="delete_btn" name="unlink" form="form_simple"
                    value="log%03d.bin">&#x274c;</button>
            </td>
        </tr>)--",
                         i);

                HANDLE(httpd_resp_send_chunk(req, buf2, HTTPD_RESP_USE_STRLEN));
            }
        }
    } else {
        HANDLE(httpd_resp_send_chunk(req, R"--(<tr><td>Logger is BUSY!</td></tr>)--",
                              HTTPD_RESP_USE_STRLEN));
    }
    HANDLE(httpd_resp_send_chunk(req, R"--(</table>


    <form id="form_simple" method="post" action=""></form>
</body>)--",
                          HTTPD_RESP_USE_STRLEN));

    HANDLE(httpd_resp_send_chunk(req, html_stylesheet, HTTPD_RESP_USE_STRLEN));
    HANDLE(httpd_resp_send_chunk(req, html_suffix, HTTPD_RESP_USE_STRLEN));
    HANDLE(httpd_resp_send_chunk(req, NULL, 0));
    return ESP_OK;
}

static const httpd_uri_t root_get = {
    .uri = "/", .method = HTTP_GET, .handler = root_get_handler, NULL};

static esp_err_t root_post_handler(httpd_req_t* req) {
    char content[100];
    size_t recv_size = MIN(req->content_len, sizeof(content) - 1);

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    if (strncmp(content, "command=record", 14) == 0) {
        ESP_LOGI(TAG, "Start logging!");
        if (!xSemaphoreTake(gctx.logger_control.mutex, 10)) {
            return ESP_FAIL;
        }

        gctx.logger_control.active = true;

        xSemaphoreGive(gctx.logger_control.mutex);
    }

    if (strncmp(content, "command=calibrate", 17) == 0) {
        ESP_LOGI(TAG, "Request calibration!");
        if (!xSemaphoreTake(gctx.logger_control.mutex, 10)) {
            return ESP_FAIL;
        }

        gctx.logger_control.calibration_pending = true;

        xSemaphoreGive(gctx.logger_control.mutex);
    }

    if (strncmp(content, "command=stop", 12) == 0) {
        ESP_LOGI(TAG, "Stop logging!");
        if (!xSemaphoreTake(gctx.logger_control.mutex, 10)) {
            return ESP_FAIL;
        }

        gctx.logger_control.active = false;

        xSemaphoreGive(gctx.logger_control.mutex);
    }

    if (strncmp(content, "unlink=", 7) == 0) {
        content[recv_size] = 0;
        std::string path = content + 7;
        if (unlink(("/spiflash/" + path).c_str())) {
            ESP_LOGE(TAG, "Could not remove '%s'", ("/spiflash/" + path).c_str());
            perror("???");
        }
    }

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");

    return httpd_resp_send(req, "", HTTPD_RESP_USE_STRLEN);
}

static const httpd_uri_t root_post = {
    .uri = "/", .method = HTTP_POST, .handler = root_post_handler, NULL};

static httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root_get);
        httpd_register_uri_handler(server, &download_get);
        httpd_register_uri_handler(server, &root_post);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static esp_err_t stop_webserver(httpd_handle_t server) { return httpd_stop(server); }

extern "C" {
void http_init() {
    static httpd_handle_t server = NULL;

    server = start_webserver();
}
}