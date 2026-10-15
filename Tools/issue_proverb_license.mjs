#!/usr/bin/env node
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const PRODUCT_ID = '92cf2454-2cfc-4db5-b210-2c732457605f';
const PLUGIN_ID = 'com.producertoy.proverb';
const KEY_ID = 'proverb-ed25519-2026-10-v2';
const TOOL_DIR = path.dirname(fileURLToPath(import.meta.url));
const PROJECT_ROOT = path.resolve(TOOL_DIR, '..');

function parseArgs(argv) {
  const values = {};
  for (let i = 0; i < argv.length; i++) {
    const key = argv[i];
    if (!key.startsWith('--') || i + 1 >= argv.length)
      throw new Error('Usage: python Tools/generate_proverb_license_file.py --device-id "<copied ID>" --name "Customer" --email "customer@example.com" [--output license.ptlic] [--key private-key.pem]');
    values[key.slice(2)] = argv[++i];
  }
  return values;
}

function maskEmail(email) {
  const parts = email.split('@');
  const local = parts[0];
  const domain = parts[1];
  const visible = local.length <= 2 ? local[0] : local[0] + '***' + local[local.length - 1];
  return visible + '@' + domain;
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  const deviceId = (args['device-id'] || '').trim().toLowerCase();
  const name = (args.name || '').trim();
  const email = (args.email || '').trim().toLowerCase();
  const defaultKey = process.env.LOCALAPPDATA
    ? path.join(process.env.LOCALAPPDATA, 'ProducerToy', 'Licensing', 'ProVerb', 'proverb-ed25519-private.pem')
    : '';
  const suppliedKey = args.key || process.env.PROVERB_SIGNING_KEY || defaultKey;
  if (!suppliedKey)
    throw new Error('Signing key is missing. Supply --key or set PROVERB_SIGNING_KEY to the protected private-key file path.');
  const keyPath = path.resolve(suppliedKey);
  const outputPath = path.resolve(args.output || path.join(process.cwd(), 'proverb-license-' + crypto.randomUUID() + '.ptlic'));

  if (!/^[0-9a-f]{2}( [0-9a-f]{2}){31}$/.test(deviceId))
    throw new Error('Device ID format is invalid. Use the Copy Device ID button in ProVerb and paste its full value.');
  if (!name || name.length > 128)
    throw new Error('Customer name is required and must be 128 characters or fewer.');
  if (!/^[^@\s]{1,254}@[^@\s]{1,253}$/.test(email))
    throw new Error('A valid customer email address is required.');
  if (!fs.existsSync(keyPath))
    throw new Error('Signing key is missing. Supply --key or set PROVERB_SIGNING_KEY to the protected private-key file path.');
  if (keyPath === PROJECT_ROOT || keyPath.startsWith(PROJECT_ROOT + path.sep))
    throw new Error('Refusing to read a signing key from inside the plugin repository.');
  if (outputPath === PROJECT_ROOT || outputPath.startsWith(PROJECT_ROOT + path.sep))
    throw new Error('Refusing to put a customer license file inside the plugin repository.');

  const key = crypto.createPrivateKey(fs.readFileSync(keyPath));
  if (key.asymmetricKeyType !== 'ed25519')
    throw new Error('The selected signing key is not an Ed25519 private key.');

  const now = Math.floor(Date.now() / 1000);
  const payload = {
    v: 2,
    iss: 'producertoy.com',
    aud: PLUGIN_ID,
    pid: PRODUCT_ID,
    plugin_id: PLUGIN_ID,
    kid: KEY_ID,
    status: 'active',
    aid: crypto.randomUUID(),
    dev_hash: deviceId,
    user_name: name,
    user_email_masked: maskEmail(email),
    tier: 'perpetual',
    iat: now,
    nbf: now,
    exp: 0
  };

  const encodedPayload = Buffer.from(JSON.stringify(payload), 'utf8').toString('base64url');
  const signature = crypto.sign(null, Buffer.from(encodedPayload, 'ascii'), key).toString('base64url');
  const token = encodedPayload + '.' + signature;

  fs.mkdirSync(path.dirname(outputPath), { recursive: true });
  fs.writeFileSync(outputPath, token, { encoding: 'utf8', flag: 'wx' });
  process.stdout.write('Created device-bound ProVerb license: ' + outputPath + '\n');
  process.stdout.write('Activation ID: ' + payload.aid + '\n');
  process.stdout.write('Copy the .ptlic file to the customer. The token and private key are not printed.\n');
}

try {
  main();
} catch (error) {
  process.stderr.write('License issuance failed: ' + error.message + '\n');
  process.exitCode = 1;
}
