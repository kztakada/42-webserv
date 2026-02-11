const fs = require('fs');

function readStdin() {
  try {
    return fs.readFileSync(0, 'utf8');
  } catch (e) {
    return '';
  }
}

const body = readStdin();
const method = process.env.REQUEST_METHOD || '';
const qs = process.env.QUERY_STRING || '';

process.stdout.write('Content-Type: text/plain\r\n');
process.stdout.write('\r\n');
process.stdout.write('node ok\n');
process.stdout.write('method=' + method + '\n');
process.stdout.write('query=' + qs + '\n');
process.stdout.write('body=' + body);
