from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
OTA = (ROOT / "src" / "ota_update.cpp").read_text(encoding="utf-8")
BUILD = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")


class OtaSocketPacingTests(unittest.TestCase):
    def test_product_marker(self):
        self.assertIn("7IN-20260802-PRODUCT65-RADAR-FRAME-CADENCE", BUILD)

    def test_control_requests_have_bounded_network_retries(self):
        page = OTA[OTA.index("const char UPDATE_PAGE[]") : OTA.index(")HTML\";")]
        self.assertIn("async function call(path,options={},attempts=3)", page)
        self.assertIn("await sleep(300*(attempt+1))", page)
        self.assertIn("if(e.httpStatus||attempt+1>=attempts)throw e", page)
        self.assertIn("options.cache='no-store'", page)

    def test_status_polling_is_paced_for_single_client_server(self):
        page = OTA[OTA.index("const char UPDATE_PAGE[]") : OTA.index(")HTML\";")]
        self.assertIn("async function waitReady(){await sleep(500);", page)
        self.assertIn("await sleep(1000)", page)
        self.assertNotIn("setTimeout(r,250)", page)
        self.assertIn("await sleep(500);const j=await upload(file)", page)

    def test_upload_retry_requires_ready_and_zero_transfer(self):
        page = OTA[OTA.index("const char UPDATE_PAGE[]") : OTA.index(")HTML\";")]
        self.assertIn("for(let attempt=0;attempt<2;attempt++)", page)
        self.assertIn("const received=Number(j.received_bytes||0)", page)
        self.assertIn("written=Number(j.written_bytes||0)", page)
        self.assertIn("j.state!=='READY'||received!==0||written!==0", page)
        self.assertIn("Upload connection lost after transfer may have started", page)
        self.assertIn("retrying once", page)

    def test_status_json_exposes_transfer_counters(self):
        send_json = OTA[OTA.index("void sendJson") : OTA.index("void resetUploadSession")]
        self.assertIn(r'\"received_bytes\"', send_json)
        self.assertIn(r'\"written_bytes\"', send_json)
        self.assertIn(r'\"firmware_bytes\"', send_json)
        self.assertIn("payloadReceived", send_json)
        self.assertIn("payloadWritten", send_json)

    def test_connection_close_header_is_not_duplicated(self):
        send_json = OTA[OTA.index("void sendJson") : OTA.index("void resetUploadSession")]
        self.assertNotIn('server.sendHeader("Connection", "close")', send_json)
        self.assertIn("WebServer already adds exactly one Connection: close header", send_json)

    def test_product57_multipart_upload_transport_is_preserved(self):
        page = OTA[OTA.index("const char UPDATE_PAGE[]") : OTA.index(")HTML\";")]
        routes = OTA[OTA.index("void configureRoutes()") : OTA.index("void stopServer()")]
        self.assertIn("x.open('POST','/upload')", page)
        self.assertIn("new FormData()", page)
        self.assertIn("f.append('firmware',file,file.name)", page)
        self.assertIn('server.on("/upload", HTTP_POST', routes)
        self.assertNotIn("/upload-raw", OTA)
        self.assertNotIn("/upload-chunk", OTA)


if __name__ == "__main__":
    unittest.main()
