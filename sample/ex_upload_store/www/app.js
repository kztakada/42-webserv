/*
  ex_upload_store: 静的サイトから upload_store に生POST

  NOTE:
  - 同一オリジンの相対URL（/upload）に POST する
  - 送信は multipart/form-data ではなく、ファイルの生バイト列をそのまま POST
*/

const UPLOAD_BASE_PATH = "/upload";

const drop = document.getElementById("drop");
const fileInput = document.getElementById("file");
const preview = document.getElementById("preview");
const log = document.getElementById("log");
const uploadUrl = document.getElementById("uploadUrl");

uploadUrl.textContent = UPLOAD_BASE_PATH + "/<filename>";

function setLog(text) {
  log.textContent = text;
}

function escapeName(name) {
  // 保存先パスの leaf に使うので、極端な文字は避ける
  return String(name)
    .replace(/\//g, "_")
    .replace(/\\/g, "_")
    .replace(/\s+/g, "_")
    .slice(0, 120);
}

function buildUploadLeaf(file) {
  const ts = new Date().toISOString().replace(/[:.]/g, "-");
  const base = file && file.name ? escapeName(file.name) : "upload.bin";
  return ts + "_" + base;
}

function showPreview(file) {
  if (!file) {
    preview.innerHTML = '<div class="preview__empty">未選択</div>';
    return;
  }
  if (!file.type || !file.type.startsWith("image/")) {
    preview.innerHTML = '<div class="preview__empty">画像ではありません</div>';
    return;
  }

  const img = document.createElement("img");
  img.alt = "preview";
  preview.innerHTML = "";
  preview.appendChild(img);

  const reader = new FileReader();
  reader.onload = () => {
    img.src = reader.result;
  };
  reader.readAsDataURL(file);
}

async function uploadFile(file) {
  if (!file) return;

  const leaf = buildUploadLeaf(file);
  const url = UPLOAD_BASE_PATH + "/" + encodeURIComponent(leaf);

  setLog(
    [
      "uploading...",
      `file: ${file.name}`,
      `type: ${file.type || "(none)"}`,
      `size: ${file.size} bytes`,
      `url:  ${url}`,
      "",
    ].join("\n")
  );

  let resp;
  try {
    resp = await fetch(url, {
      method: "POST",
      headers: {
        // 任意: 何を送っているか分かりやすいように付ける
        "Content-Type": file.type || "application/octet-stream",
        "X-Original-Filename": file.name || "",
      },
      body: file,
    });
  } catch (e) {
    setLog("fetch failed: " + String(e));
    return;
  }

  const text = await resp.text().catch(() => "");

  setLog(
    [
      "done",
      `status: ${resp.status} ${resp.statusText}`,
      "",
      "response body:",
      text,
      "",
      "保存先は conf の upload_store 配下です（README 参照）。",
    ].join("\n")
  );
}

function handleFiles(files) {
  if (!files || files.length === 0) return;
  const file = files[0];
  showPreview(file);
  uploadFile(file);
}

// Drag & Drop
["dragenter", "dragover"].forEach((ev) => {
  drop.addEventListener(ev, (e) => {
    e.preventDefault();
    e.stopPropagation();
    drop.classList.add("is-dragover");
  });
});

["dragleave", "drop"].forEach((ev) => {
  drop.addEventListener(ev, (e) => {
    e.preventDefault();
    e.stopPropagation();
    drop.classList.remove("is-dragover");
  });
});

drop.addEventListener("drop", (e) => {
  const dt = e.dataTransfer;
  if (!dt) return;
  handleFiles(dt.files);
});

// File input
fileInput.addEventListener("change", (e) => {
  const files = e.target.files;
  handleFiles(files);
});
