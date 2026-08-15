import { ESPLoader, Transport } from "https://unpkg.com/esptool-js@0.6.0/bundle.js";

const EXPECTED = Object.freeze({
  schema: 1,
  hardware: "waveshare-esp32-s3-touch-lcd-7",
  chip: "esp32s3",
  chipName: "ESP32-S3",
  flashSize: "16MB",
  flashMode: "dio",
  flashFreq: "80m",
  distributionMarker: "RADAR-DISTRIBUTION-BUILD",
  confirmation: "ERASE RADAR",
  esptoolJsVersion: "0.6.0",
  layout: Object.freeze([
    Object.freeze({ name: "bootloader.bin", address: 0x00000000 }),
    Object.freeze({ name: "partitions.bin", address: 0x00008000 }),
    Object.freeze({ name: "boot_app0.bin", address: 0x0000e000 }),
    Object.freeze({ name: "firmware.bin", address: 0x00010000 }),
  ]),
});

const state = {
  manifest: null,
  images: null,
  port: null,
  transport: null,
  loader: null,
  deviceVerified: false,
  busy: false,
};

const el = {
  bundleFiles: document.getElementById("bundleFiles"),
  loadAdjacent: document.getElementById("loadAdjacent"),
  connect: document.getElementById("connectButton"),
  disconnect: document.getElementById("disconnectButton"),
  eraseCheck: document.getElementById("eraseCheck"),
  eraseText: document.getElementById("eraseText"),
  install: document.getElementById("installButton"),
  bundleStatus: document.getElementById("bundleStatus"),
  deviceStatus: document.getElementById("deviceStatus"),
  installStatus: document.getElementById("installStatus"),
  progress: document.getElementById("progress"),
  facts: document.getElementById("facts"),
  log: document.getElementById("log"),
};

function log(message) {
  const stamp = new Date().toLocaleTimeString();
  el.log.textContent += `\n[${stamp}] ${message}`;
  el.log.scrollTop = el.log.scrollHeight;
}

function setStatus(target, text, kind = "") {
  target.textContent = text;
  target.classList.remove("ok", "bad");
  if (kind) target.classList.add(kind);
}

function setBusy(busy) {
  state.busy = busy;
  el.bundleFiles.disabled = busy;
  el.loadAdjacent.disabled = busy;
  refreshControls();
}

function refreshControls() {
  const bundleReady = Boolean(state.manifest && state.images);
  const serialReady = Boolean(window.isSecureContext && navigator.serial);
  const confirmed = el.eraseCheck.checked && el.eraseText.value.trim() === EXPECTED.confirmation;
  el.connect.disabled = state.busy || !bundleReady || !serialReady || state.deviceVerified;
  el.disconnect.disabled = state.busy || !state.transport;
  el.install.disabled = state.busy || !bundleReady || !state.deviceVerified || !confirmed;
}

function assert(condition, message) {
  if (!condition) throw new Error(message);
}

function parseAddress(value) {
  assert(typeof value === "string" && /^0x[0-9a-fA-F]{8}$/.test(value), "Manifest contains an invalid flash address.");
  return Number.parseInt(value.slice(2), 16);
}

function bytesContainAscii(bytes, text) {
  const needle = new TextEncoder().encode(text);
  if (!needle.length || needle.length > bytes.length) return false;
  outer: for (let i = 0; i <= bytes.length - needle.length; ++i) {
    for (let j = 0; j < needle.length; ++j) {
      if (bytes[i + j] !== needle[j]) continue outer;
    }
    return true;
  }
  return false;
}

async function sha256Hex(bytes) {
  const digest = new Uint8Array(await crypto.subtle.digest("SHA-256", bytes));
  return Array.from(digest, (value) => value.toString(16).padStart(2, "0")).join("");
}

function md5Hex(bytes) {
  const shifts = [7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21];
  const constants = Array.from({ length: 64 }, (_, i) =>
    Math.floor(Math.abs(Math.sin(i + 1)) * 0x100000000) >>> 0
  );
  const paddedLength = Math.ceil((bytes.length + 9) / 64) * 64;
  const padded = new Uint8Array(paddedLength);
  padded.set(bytes);
  padded[bytes.length] = 0x80;
  const view = new DataView(padded.buffer);
  const bitLengthLow = (bytes.length << 3) >>> 0;
  const bitLengthHigh = Math.floor(bytes.length / 0x20000000) >>> 0;
  view.setUint32(paddedLength - 8, bitLengthLow, true);
  view.setUint32(paddedLength - 4, bitLengthHigh, true);

  let a0 = 0x67452301;
  let b0 = 0xefcdab89;
  let c0 = 0x98badcfe;
  let d0 = 0x10325476;

  const rotateLeft = (value, count) => ((value << count) | (value >>> (32 - count))) >>> 0;

  for (let offset = 0; offset < paddedLength; offset += 64) {
    const words = new Uint32Array(16);
    for (let i = 0; i < 16; ++i) words[i] = view.getUint32(offset + i * 4, true);

    let a = a0;
    let b = b0;
    let c = c0;
    let d = d0;

    for (let i = 0; i < 64; ++i) {
      let f;
      let g;
      if (i < 16) {
        f = (b & c) | (~b & d);
        g = i;
      } else if (i < 32) {
        f = (d & b) | (~d & c);
        g = (5 * i + 1) & 15;
      } else if (i < 48) {
        f = b ^ c ^ d;
        g = (3 * i + 5) & 15;
      } else {
        f = c ^ (b | ~d);
        g = (7 * i) & 15;
      }
      const shift = shifts[(i >> 4) * 4 + (i & 3)];
      const nextD = c;
      c = b;
      const sum = (a + f + constants[i] + words[g]) >>> 0;
      b = (b + rotateLeft(sum, shift)) >>> 0;
      a = d;
      d = nextD;
    }

    a0 = (a0 + a) >>> 0;
    b0 = (b0 + b) >>> 0;
    c0 = (c0 + c) >>> 0;
    d0 = (d0 + d) >>> 0;
  }

  const digest = new Uint8Array(16);
  const digestView = new DataView(digest.buffer);
  digestView.setUint32(0, a0, true);
  digestView.setUint32(4, b0, true);
  digestView.setUint32(8, c0, true);
  digestView.setUint32(12, d0, true);
  return Array.from(digest, (value) => value.toString(16).padStart(2, "0")).join("");
}

function validateManifest(manifest) {
  assert(manifest && typeof manifest === "object", "Factory manifest is not an object.");
  assert(manifest.schema === EXPECTED.schema, "Unsupported factory manifest schema.");
  assert(manifest.hardware === EXPECTED.hardware, "Factory manifest hardware mismatch.");
  assert(manifest.chip === EXPECTED.chip, "Factory manifest chip mismatch.");
  assert(manifest.flash_size === EXPECTED.flashSize, "Factory manifest flash-size mismatch.");
  assert(manifest.flash_mode === EXPECTED.flashMode, "Factory manifest flash-mode mismatch.");
  assert(manifest.flash_freq === EXPECTED.flashFreq, "Factory manifest flash-frequency mismatch.");
  assert(manifest.distribution_marker === EXPECTED.distributionMarker, "Distribution provenance marker mismatch.");
  assert(manifest.destructive_full_erase === true, "Manifest does not authorize a complete factory erase.");
  assert(manifest.browser_installer && manifest.browser_installer.html === "INSTALL_RADAR.html", "Browser installer HTML metadata mismatch.");
  assert(manifest.browser_installer.esptool_js_version === EXPECTED.esptoolJsVersion, "Browser installer esptool-js version mismatch.");
  assert(Number.isInteger(manifest.version_code) && manifest.version_code > 0, "Manifest Product number is invalid.");
  assert(typeof manifest.version_label === "string" && manifest.version_label === `Product ${manifest.version_code}`, "Manifest Product label is invalid.");
  assert(typeof manifest.build_id === "string" && /^7IN-[A-Z0-9-]+$/.test(manifest.build_id), "Manifest build ID is invalid.");
  assert(Array.isArray(manifest.files) && manifest.files.length === EXPECTED.layout.length, "Factory manifest file count is invalid.");

  const byName = new Map();
  for (const entry of manifest.files) {
    assert(entry && typeof entry === "object", "Factory manifest contains an invalid file entry.");
    assert(typeof entry.name === "string" && EXPECTED.layout.some((item) => item.name === entry.name), `Unexpected factory file: ${entry.name}`);
    assert(!byName.has(entry.name), `Duplicate factory file: ${entry.name}`);
    assert(Number.isInteger(entry.size) && entry.size > 0, `Invalid size for ${entry.name}.`);
    assert(typeof entry.sha256 === "string" && /^[0-9a-f]{64}$/i.test(entry.sha256), `Invalid SHA-256 for ${entry.name}.`);
    byName.set(entry.name, entry);
  }

  for (const expected of EXPECTED.layout) {
    const entry = byName.get(expected.name);
    assert(entry, `Required factory file missing from manifest: ${expected.name}`);
    assert(parseAddress(entry.address) === expected.address, `Unexpected flash address for ${expected.name}.`);
  }
  return byName;
}

async function verifyBundle(manifest, fileBytes) {
  const entries = validateManifest(manifest);
  const images = [];
  for (const expected of EXPECTED.layout) {
    const entry = entries.get(expected.name);
    const bytes = fileBytes.get(expected.name);
    assert(bytes instanceof Uint8Array, `Missing required factory file: ${expected.name}`);
    assert(bytes.length === entry.size, `Size mismatch for ${expected.name}.`);
    const digest = await sha256Hex(bytes);
    assert(digest.toLowerCase() === entry.sha256.toLowerCase(), `SHA-256 mismatch for ${expected.name}.`);
    images.push({ name: expected.name, address: expected.address, data: bytes });
  }

  const firmware = fileBytes.get("firmware.bin");
  assert(bytesContainAscii(firmware, EXPECTED.distributionMarker), "firmware.bin is not a distribution build.");
  assert(bytesContainAscii(firmware, manifest.build_id), "firmware.bin does not contain the manifest build ID.");
  return images;
}

function updateFacts(manifest) {
  el.facts.innerHTML = "";
  const rows = [
    ["Product", manifest.version_label],
    ["Build", manifest.build_id],
    ["Hardware", manifest.hardware],
    ["Required chip", "ESP32-S3"],
    ["Required flash", manifest.flash_size],
    ["Erase policy", "FULL CHIP ERASE"],
    ["esptool-js", manifest.browser_installer?.esptool_js_version || "0.6.0"],
  ];
  for (const [name, value] of rows) {
    const key = document.createElement("span");
    const val = document.createElement("span");
    key.textContent = name;
    val.textContent = value;
    el.facts.append(key, val);
  }
}

async function acceptBundle(manifest, bytesByName) {
  setStatus(el.bundleStatus, "Verifying factory bundle...");
  const images = await verifyBundle(manifest, bytesByName);
  state.manifest = manifest;
  state.images = images;
  state.deviceVerified = false;
  updateFacts(manifest);
  setStatus(el.bundleStatus, `${manifest.version_label} verified\n${manifest.build_id}\n4/4 images passed size + SHA-256 + provenance checks.`, "ok");
  log(`Factory bundle verified: ${manifest.build_id}`);
  refreshControls();
}

async function loadFromSelection(fileList) {
  const files = Array.from(fileList || []);
  assert(files.length > 0, "No factory bundle files were selected.");
  const byBaseName = new Map();
  for (const file of files) {
    if (byBaseName.has(file.name) && (file.name === "factory-manifest.json" || EXPECTED.layout.some((item) => item.name === file.name))) {
      throw new Error(`Duplicate required factory filename in selected folder: ${file.name}`);
    }
    if (!byBaseName.has(file.name)) byBaseName.set(file.name, file);
  }
  const manifestFile = byBaseName.get("factory-manifest.json");
  assert(manifestFile, "factory-manifest.json was not found in the selected folder.");
  const manifest = JSON.parse(await manifestFile.text());
  const bytes = new Map();
  for (const expected of EXPECTED.layout) {
    const file = byBaseName.get(expected.name);
    assert(file, `Selected folder is missing ${expected.name}.`);
    bytes.set(expected.name, new Uint8Array(await file.arrayBuffer()));
  }
  await acceptBundle(manifest, bytes);
}

async function loadAdjacentBundle() {
  const manifestResponse = await fetch("./factory-manifest.json", { cache: "no-store" });
  assert(manifestResponse.ok, `factory-manifest.json returned HTTP ${manifestResponse.status}.`);
  const manifest = await manifestResponse.json();
  validateManifest(manifest);
  const bytes = new Map();
  for (const expected of EXPECTED.layout) {
    const response = await fetch(`./${expected.name}`, { cache: "no-store" });
    assert(response.ok, `${expected.name} returned HTTP ${response.status}.`);
    bytes.set(expected.name, new Uint8Array(await response.arrayBuffer()));
  }
  await acceptBundle(manifest, bytes);
}

const terminal = {
  clean() {},
  writeLine(data) { log(String(data)); },
  write(data) { log(String(data)); },
};

async function releaseSerialTransport({ updateStatus = true } = {}) {
  const transport = state.transport;
  if (transport) {
    // Explicitly release the USB-UART reset/boot control lines before closing
    // Web Serial. Product 95 field testing showed that a browser session could
    // otherwise leave this Waveshare board held until a manual reset.
    try { await transport.setDTR(false); } catch (error) { log(`DTR release warning: ${error}`); }
    try { await transport.setRTS(false); } catch (error) { log(`RTS release warning: ${error}`); }
    await new Promise((resolve) => setTimeout(resolve, 150));
    try {
      await transport.disconnect();
      log("Web Serial transport released.");
    } catch (error) {
      log(`Disconnect warning: ${error}`);
    }
  }
  state.port = null;
  state.transport = null;
  state.loader = null;
  state.deviceVerified = false;
  if (updateStatus) setStatus(el.deviceStatus, "No radar connected.");
  refreshControls();
}

async function disconnect() {
  await releaseSerialTransport({ updateStatus: true });
}

async function connectAndVerify() {
  assert(window.isSecureContext, "Web Serial requires a secure context. Use HTTPS, localhost, or a browser that treats local files as secure.");
  assert(navigator.serial, "Web Serial is unavailable. Use current Chrome or Edge on a supported desktop system.");
  assert(state.manifest && state.images, "Load and verify the factory bundle first.");

  await disconnect();
  const port = await navigator.serial.requestPort();
  const transport = new Transport(port, true);
  const loader = new ESPLoader({ transport, baudrate: 921600, terminal, debugLogging: false });
  state.port = port;
  state.transport = transport;
  state.loader = loader;

  setStatus(el.deviceStatus, "Connecting to radar...");
  const description = await loader.main();
  log(`Detected chip description: ${description}`);
  assert(loader.chip && loader.chip.CHIP_NAME === EXPECTED.chipName, `Connected chip is ${loader.chip?.CHIP_NAME || "unknown"}; ESP32-S3 required.`);
  const flashSize = await loader.detectFlashSize();
  assert(flashSize === EXPECTED.flashSize, `Detected flash size is ${flashSize}; 16MB required.`);

  state.deviceVerified = true;
  setStatus(el.deviceStatus, `Hardware guard PASS\n${description}\nDetected flash: ${flashSize}`, "ok");
  log("Hardware guard PASS: ESP32-S3 + 16MB flash.");
  refreshControls();
}

async function eraseAndInstall() {
  assert(state.loader && state.transport && state.deviceVerified, "Verified radar connection is required.");
  assert(state.manifest && state.images, "Verified factory bundle is required.");
  assert(el.eraseCheck.checked && el.eraseText.value.trim() === EXPECTED.confirmation, "Destructive confirmation is incomplete.");

  setBusy(true);
  el.progress.value = 0;
  try {
    setStatus(el.installStatus, "ERASING ENTIRE 16 MB FLASH...", "bad");
    log("Beginning explicit full-chip erase.");
    await state.loader.eraseFlash();
    log("Full-chip erase completed.");

    setStatus(el.installStatus, "Writing verified distribution factory images...");
    const fileArray = state.images.map((image) => ({ data: image.data, address: image.address }));
    await state.loader.writeFlash({
      fileArray,
      flashMode: EXPECTED.flashMode,
      flashFreq: EXPECTED.flashFreq,
      flashSize: EXPECTED.flashSize,
      eraseAll: false,
      compress: true,
      calculateMD5Hash: md5Hex,
      reportProgress(fileIndex, written, total) {
        const fileFraction = total > 0 ? written / total : 0;
        const overall = ((fileIndex + fileFraction) / fileArray.length) * 100;
        el.progress.value = Math.max(0, Math.min(100, overall));
        const name = state.images[fileIndex]?.name || `image ${fileIndex + 1}`;
        setStatus(el.installStatus, `Writing ${name}... ${Math.floor(fileFraction * 100)}%`);
      },
    });

    el.progress.value = 100;
    setStatus(el.installStatus, "Factory image verified. Resetting radar and releasing USB...", "ok");
    log("All factory images written successfully. Hard resetting radar.");
    await state.loader.after("hard_reset");
    await new Promise((resolve) => setTimeout(resolve, 250));
    log("Releasing reset/boot control lines and closing Web Serial.");
    await releaseSerialTransport({ updateStatus: false });
    setStatus(el.deviceStatus, "USB serial released.", "ok");
    setStatus(el.installStatus, `${state.manifest.version_label} FACTORY INSTALL COMPLETE\nUSB serial has been released. The radar should boot into new-owner setup. If the display remains stopped, press RESET once.`, "ok");
    log("Factory install complete; Web Serial released.");
  } catch (error) {
    setStatus(el.installStatus, `FACTORY INSTALL FAILED\n${error instanceof Error ? error.message : String(error)}`, "bad");
    log(`Factory install failed: ${error}`);
    await disconnect();
    throw error;
  } finally {
    setBusy(false);
  }
}

el.bundleFiles.addEventListener("change", async () => {
  try {
    setBusy(true);
    await loadFromSelection(el.bundleFiles.files);
  } catch (error) {
    state.manifest = null;
    state.images = null;
    setStatus(el.bundleStatus, `Bundle verification FAILED\n${error instanceof Error ? error.message : String(error)}`, "bad");
    log(`Bundle verification failed: ${error}`);
  } finally {
    setBusy(false);
  }
});

el.loadAdjacent.addEventListener("click", async () => {
  try {
    setBusy(true);
    await loadAdjacentBundle();
  } catch (error) {
    state.manifest = null;
    state.images = null;
    setStatus(el.bundleStatus, `Adjacent bundle load FAILED\n${error instanceof Error ? error.message : String(error)}`, "bad");
    log(`Adjacent bundle load failed: ${error}`);
  } finally {
    setBusy(false);
  }
});

el.connect.addEventListener("click", async () => {
  try {
    setBusy(true);
    await connectAndVerify();
  } catch (error) {
    setStatus(el.deviceStatus, `Hardware verification FAILED\n${error instanceof Error ? error.message : String(error)}`, "bad");
    log(`Hardware verification failed: ${error}`);
    await disconnect();
  } finally {
    setBusy(false);
  }
});

el.disconnect.addEventListener("click", disconnect);
el.eraseCheck.addEventListener("change", refreshControls);
el.eraseText.addEventListener("input", refreshControls);
el.install.addEventListener("click", async () => {
  try {
    await eraseAndInstall();
  } catch (_) {
    // Error is already surfaced in the status/log. No automatic retry after a
    // destructive operation; the user must reconnect and deliberately retry.
  }
});

window.addEventListener("beforeunload", () => {
  if (!state.transport) return;
  state.transport.setDTR(false).catch(() => {});
  state.transport.setRTS(false).catch(() => {});
  state.transport.disconnect().catch(() => {});
});

if (!window.isSecureContext || !navigator.serial) {
  setStatus(el.deviceStatus, "Web Serial unavailable in this browser/context. Use current Chrome or Edge over HTTPS/localhost, or try the local file in a browser that exposes Web Serial for file://.", "bad");
}
refreshControls();
