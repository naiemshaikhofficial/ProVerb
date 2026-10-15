#!/usr/bin/env node
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const PRODUCT_ID = '92cf2454-2cfc-4db5-b210-2c732457605f';
const PLUGIN_ID = 'com.producertoy.proverb';
const KEY_ID = 'proverb-ed25519-2026-10-v2';
const PUBLIC_KEY_HEX = '874392bb5c67a3e2d91af3bf4d4de8204b39e85684df76900f87a880c8fbd2e9';

const keyPath = path.join(process.env.LOCALAPPDATA, 'ProducerToy', 'Licensing', 'ProVerb', 'proverb-ed25519-private.pem');
if (!fs.existsSync(keyPath)) {
  console.error('Private key not found at:', keyPath);
  process.exit(1);
}

const privateKey = crypto.createPrivateKey(fs.readFileSync(keyPath));

function createToken(payloadOverrides = {}) {
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
    dev_hash: 'a'.repeat(64),
    user_name: 'Test Studio Producer',
    user_email_masked: 't***r@producer.com',
    tier: 'perpetual',
    iat: now,
    nbf: now,
    exp: 0,
    ...payloadOverrides
  };

  const encodedPayload = Buffer.from(JSON.stringify(payload), 'utf8').toString('base64url');
  const signature = crypto.sign(null, Buffer.from(encodedPayload, 'ascii'), privateKey).toString('base64url');
  return encodedPayload + '.' + signature;
}

function verifyTokenWithPubKey(token, expectedDevId = 'a'.repeat(64)) {
  const parts = token.split('.');
  if (parts.length !== 2) return { valid: false, reason: 'malformed_structure' };
  const [encodedPayload, encodedSig] = parts;

  // Signature check
  const pubKeyObj = crypto.createPublicKey({
    key: Buffer.concat([
      Buffer.from('302a300506032b6570032100', 'hex'), // Ed25519 SPKI DER prefix
      Buffer.from(PUBLIC_KEY_HEX, 'hex')
    ]),
    format: 'der',
    type: 'spki'
  });

  const sigValid = crypto.verify(
    null,
    Buffer.from(encodedPayload, 'ascii'),
    pubKeyObj,
    Buffer.from(encodedSig, 'base64url')
  );
  if (!sigValid) return { valid: false, reason: 'invalid_signature' };

  const payload = JSON.parse(Buffer.from(encodedPayload, 'base64url').toString('utf8'));
  const now = Math.floor(Date.now() / 1000);

  if (payload.v !== 2) return { valid: false, reason: 'version_mismatch' };
  if (payload.iss !== 'producertoy.com') return { valid: false, reason: 'issuer_mismatch' };
  if (payload.aud !== PLUGIN_ID) return { valid: false, reason: 'audience_mismatch' };
  if (payload.pid !== PRODUCT_ID) return { valid: false, reason: 'product_mismatch' };
  if (payload.plugin_id !== PLUGIN_ID) return { valid: false, reason: 'plugin_id_mismatch' };
  if (payload.kid !== KEY_ID) return { valid: false, reason: 'key_id_mismatch' };
  if (payload.status !== 'active') return { valid: false, reason: 'not_active' };
  if (payload.tier !== 'perpetual') return { valid: false, reason: 'tier_mismatch' };
  if (payload.dev_hash !== expectedDevId) return { valid: false, reason: 'device_mismatch' };
  if (payload.iat > now + 300) return { valid: false, reason: 'future_issue' };
  if (payload.nbf > now + 300) return { valid: false, reason: 'not_before' };
  if (payload.exp !== 0 && now > payload.exp) return { valid: false, reason: 'expired' };

  return { valid: true, payload };
}

console.log('=== PROVERB END-TO-END CRYPTOGRAPHIC & CLAIM VERIFICATION TEST ===');

// Test 1: Valid Token
const validToken = createToken();
const test1 = verifyTokenWithPubKey(validToken);
console.log('Test 1 (Valid Token):', test1.valid ? 'PASSED' : 'FAILED', test1.reason || '');

// Test 2: Tampered Payload
const tamperedToken = validToken.slice(0, 10) + 'X' + validToken.slice(11);
const test2 = verifyTokenWithPubKey(tamperedToken);
console.log('Test 2 (Tampered Payload Rejected):', !test2.valid && test2.reason === 'invalid_signature' ? 'PASSED' : 'FAILED', test2.reason);

// Test 3: Wrong Device ID
const test3 = verifyTokenWithPubKey(validToken, 'b'.repeat(64));
console.log('Test 3 (Wrong Device Rejected):', !test3.valid && test3.reason === 'device_mismatch' ? 'PASSED' : 'FAILED', test3.reason);

// Test 4: Wrong Product ID
const wrongProdToken = createToken({ pid: '11111111-2cfc-4db5-b210-2c732457605f' });
const test4 = verifyTokenWithPubKey(wrongProdToken);
console.log('Test 4 (Wrong Product Rejected):', !test4.valid && test4.reason === 'product_mismatch' ? 'PASSED' : 'FAILED', test4.reason);

// Test 5: Wrong Plugin ID
const wrongPluginToken = createToken({ plugin_id: 'com.producertoy.other' });
const test5 = verifyTokenWithPubKey(wrongPluginToken);
console.log('Test 5 (Wrong Plugin ID Rejected):', !test5.valid && test5.reason === 'plugin_id_mismatch' ? 'PASSED' : 'FAILED', test5.reason);

// Test 6: Expired Token
const expiredToken = createToken({ exp: Math.floor(Date.now() / 1000) - 3600 });
const test6 = verifyTokenWithPubKey(expiredToken);
console.log('Test 6 (Expired Token Rejected):', !test6.valid && test6.reason === 'expired' ? 'PASSED' : 'FAILED', test6.reason);

// Test 7: Revoked Status
const revokedToken = createToken({ status: 'revoked' });
const test7 = verifyTokenWithPubKey(revokedToken);
console.log('Test 7 (Revoked Status Rejected):', !test7.valid && test7.reason === 'not_active' ? 'PASSED' : 'FAILED', test7.reason);

// Test 8: Foreign/Forged Signature with different Ed25519 key
const foreignKey = crypto.generateKeyPairSync('ed25519').privateKey;
const payloadStr = Buffer.from(JSON.stringify({ v: 2, iss: 'producertoy.com', aud: PLUGIN_ID, pid: PRODUCT_ID, plugin_id: PLUGIN_ID, kid: KEY_ID, status: 'active', dev_hash: 'a'.repeat(64), aid: '1', user_name: 'Hacker', user_email_masked: 'h@h.com', tier: 'perpetual', iat: 100, nbf: 100, exp: 0 }), 'utf8').toString('base64url');
const forgedSig = crypto.sign(null, Buffer.from(payloadStr, 'ascii'), foreignKey).toString('base64url');
const forgedToken = payloadStr + '.' + forgedSig;
const test8 = verifyTokenWithPubKey(forgedToken);
console.log('Test 8 (Forged Foreign Signature Rejected):', !test8.valid && test8.reason === 'invalid_signature' ? 'PASSED' : 'FAILED', test8.reason);

console.log('==================================================================');
