#!/usr/bin/node

function main() {
  const chunks = [];
  process.stdin.on("data", (c) => chunks.push(c));
  process.stdin.on("end", () => {
    const data = Buffer.concat(chunks);
    const method = process.env.REQUEST_METHOD || "";
    let nul = 0;
    for (let i = 0; i < data.length; i++) {
      if (data[i] === 0) nul++;
    }

    process.stdout.write("Content-Type: text/plain\r\n");
    process.stdout.write("\r\n");
    process.stdout.write("node meta\n");
    process.stdout.write("method=" + method + "\n");
    process.stdout.write("body_len=" + String(data.length) + "\n");
    process.stdout.write("nul_bytes=" + String(nul) + "\n");
  });
}

main();
