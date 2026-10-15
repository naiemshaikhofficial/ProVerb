#!/usr/bin/env node
/**
 * Automated Production End-to-End Test Suite for ProVerb Dedicated Licensing Server
 * Tests:
 * 1. Health check & configuration
 * 2. Razorpay payment webhook (HMAC signature, automatic serial generation)
 * 3. Invalid webhook signature rejection
 * 4. Webhook idempotency (duplicate retry protection)
 * 5. Plugin serial redemption (Ed25519 token issuance)
 * 6. Idempotent re-activation on same hardware device
 * 7. Multi-seat limit enforcement (3 seats max)
 * 8. Deactivation and seat release
 * 9. Refund webhook and automatic license revocation
 * 10. Offline license file generation
 */

import http from 'node:http';
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const PORT = 4099;
const HOST = '127.0.0.1';
const BASE_URL = `http://${HOST}:${PORT}`;

const PRODUCT_ID = '92cf2454-2cfc-4db5-b210-2c732457605f';
const TEST_WEBHOOK_SECRET = 'rzp_test_webhook_secret_key_8923489234';
const TEST_SALT_SECRET = 'test_salt_secret_high_entropy_32_characters_minimum!';
const TEST_ADMIN_KEY = 'test_admin_secret_key_12345';

// Set test environment variables
process.env.PORT = String(PORT);
process.env.HOST = HOST;
process.env.LICENSING_SALT_SECRET = TEST_SALT_SECRET;
process.env.LICENSING_ADMIN_KEY = TEST_ADMIN_KEY;
process.env.RAZORPAY_WEBHOOK_SECRET = TEST_WEBHOOK_SECRET;

// Ensure local private key is accessible
const localKeyPath = path.join(process.env.LOCALAPPDATA, 'ProducerToy', 'Licensing', 'ProVerb', 'proverb-ed25519-private.pem');
process.env.PROVERB_SIGNING_KEY = localKeyPath;

// Use temporary isolated data dir for tests
const TEST_DATA_DIR = path.join(__dirname, '..', 'build', 'test_licensing_data');
if (fs.existsSync(TEST_DATA_DIR)) {
  fs.rmSync(TEST_DATA_DIR, { recursive: true, force: true });
}
fs.mkdirSync(TEST_DATA_DIR, { recursive: true });
process.env.DATA_DIR = TEST_DATA_DIR;

// Helper to make HTTP requests
function httpRequest(method, endpoint, headers = {}, body = null) {
  return new Promise((resolve, reject) => {
    const url = new URL(endpoint, BASE_URL);
    const options = {
      method,
      hostname: url.hostname,
      port: url.port,
      path: url.pathname + url.search,
      headers: {
        'Content-Type': 'application/json',
        ...headers
      }
    };

    const req = http.request(options, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => {
        let json = null;
        try { json = JSON.parse(data); } catch (e) { json = data; }
        resolve({ status: res.statusCode, headers: res.headers, body: json });
      });
    });

    req.on('error', reject);
    if (body) {
      req.write(typeof body === 'string' ? body : JSON.stringify(body));
    }
    req.end();
  });
}

async function runTests() {
  console.log('===============================================================');
  console.log(' ProVerb Licensing Server Automated Test Suite');
  console.log('===============================================================');

  // Import and start server
  await import('../LicensingServer/server.mjs');
  await new Promise(r => setTimeout(r, 600)); // wait for listen

  let passed = 0;
  let failed = 0;

  function assert(condition, message) {
    if (condition) {
      console.log(`[PASS] ${message}`);
      passed++;
    } else {
      console.error(`[FAIL] ${message}`);
      failed++;
    }
  }

  try {
    // 1. Health check probe
    const health = await httpRequest('GET', '/health');
    assert(health.status === 200 && health.body.status === 'ready', 'Health endpoint reports status ready');
    assert(health.body.signingKeyLoaded === true, 'Ed25519 private signing key successfully loaded');
    assert(health.body.databaseHealthy === true, 'SQLite transactional database verified healthy');

    // 2. Razorpay Webhook with valid signature -> issues serial key
    const orderId = `ord_test_${Date.now()}`;
    const paymentId = `pay_test_${Date.now()}`;
    const eventId = `evt_test_${Date.now()}`;

    const webhookPayload = {
      event: 'payment.captured',
      event_id: eventId,
      payload: {
        payment: {
          entity: {
            id: paymentId,
            order_id: orderId,
            amount: 999,
            currency: 'USD',
            status: 'captured',
            email: 'studio.producer@gmail.com',
            notes: {
              userName: 'Naiem Shaikh',
              userEmail: 'studio.producer@gmail.com'
            }
          }
        }
      }
    };

    const rawBody = JSON.stringify(webhookPayload);
    const validSignature = crypto.createHmac('sha256', TEST_WEBHOOK_SECRET).update(rawBody).digest('hex');

    const webhookRes = await httpRequest('POST', '/api/v1/license/webhook/razorpay', {
      'X-Razorpay-Signature': validSignature
    }, rawBody);

    assert(webhookRes.status === 200 && webhookRes.body.success === true, 'Signed Razorpay webhook processes successfully');
    const issuedSerial = webhookRes.body.serialKey;
    assert(typeof issuedSerial === 'string' && issuedSerial.startsWith('PT-PROV-'), `Cryptographic serial issued: ${issuedSerial}`);

    // 3. Invalid signature rejected
    const invalidSigRes = await httpRequest('POST', '/api/v1/license/webhook/razorpay', {
      'X-Razorpay-Signature': 'invalid_forged_hex_signature_1234567890'
    }, rawBody);
    assert(invalidSigRes.status === 401, 'Forged webhook signature rejected with 401 Unauthorized');

    // 4. Duplicate webhook idempotency
    const retryRes = await httpRequest('POST', '/api/v1/license/webhook/razorpay', {
      'X-Razorpay-Signature': validSignature
    }, rawBody);
    assert(retryRes.status === 200 && retryRes.body.status === 'already_processed', 'Duplicate webhook retries handled idempotently without re-issuing');

    // 5. Plugin Serial Key Activation
    const device1 = '1111111111111111111111111111111111111111111111111111111111111111';
    const actRes1 = await httpRequest('POST', '/api/v1/license/serial/activate', {}, {
      productId: PRODUCT_ID,
      serialKey: issuedSerial,
      deviceId: device1,
      deviceName: 'Main Studio PC',
      os: 'Windows'
    });
    assert(actRes1.status === 200 && actRes1.body.success === true, 'Device 1 activated with serial key');
    assert(typeof actRes1.body.token === 'string' && actRes1.body.token.includes('.'), 'Valid Ed25519 token issued');
    assert(actRes1.body.seatsUsed === 1 && actRes1.body.maxSeats === 3, 'Seat count updated to 1/3');

    // 6. Idempotent re-activation on same device
    const actRes1Repeat = await httpRequest('POST', '/api/v1/license/serial/activate', {}, {
      productId: PRODUCT_ID,
      serialKey: issuedSerial,
      deviceId: device1,
      deviceName: 'Main Studio PC'
    });
    assert(actRes1Repeat.status === 200 && actRes1Repeat.body.token === actRes1.body.token, 'Same device re-activation returns existing token idempotently');
    assert(actRes1Repeat.body.seatsUsed === 1, 'Seat count remains 1/3 (not incremented)');

    // 7. Activate Device 2 and Device 3 (fill 3 seats)
    const device2 = '2222222222222222222222222222222222222222222222222222222222222222';
    const actRes2 = await httpRequest('POST', '/api/v1/license/serial/activate', {}, {
      productId: PRODUCT_ID,
      serialKey: issuedSerial,
      deviceId: device2,
      deviceName: 'MacBook Pro M3',
      os: 'macOS'
    });
    assert(actRes2.status === 200 && actRes2.body.seatsUsed === 2, 'Device 2 activated (2/3 seats)');

    const device3 = '3333333333333333333333333333333333333333333333333333333333333333';
    const actRes3 = await httpRequest('POST', '/api/v1/license/serial/activate', {}, {
      productId: PRODUCT_ID,
      serialKey: issuedSerial,
      deviceId: device3,
      deviceName: 'Studio B Laptop'
    });
    assert(actRes3.status === 200 && actRes3.body.seatsUsed === 3, 'Device 3 activated (3/3 seats full)');

    // 8. 4th device activation exceeds seat limit
    const device4 = '4444444444444444444444444444444444444444444444444444444444444444';
    const actRes4 = await httpRequest('POST', '/api/v1/license/serial/activate', {}, {
      productId: PRODUCT_ID,
      serialKey: issuedSerial,
      deviceId: device4
    });
    assert(actRes4.status === 403, '4th device activation rejected with 403 Seat Limit Reached');

    // 9. Deactivate Device 2 -> Seat released
    const deactRes = await httpRequest('POST', '/api/v1/license/deactivate', {}, {
      productId: PRODUCT_ID,
      deviceId: device2
    });
    assert(deactRes.status === 200 && deactRes.body.deactivated === true, 'Device 2 deactivated and seat released on server');

    // 10. Device 4 can now activate into the freed seat
    const actRes4After = await httpRequest('POST', '/api/v1/license/serial/activate', {}, {
      productId: PRODUCT_ID,
      serialKey: issuedSerial,
      deviceId: device4,
      deviceName: 'Studio B Upgraded'
    });
    assert(actRes4After.status === 200 && actRes4After.body.seatsUsed === 3, 'Device 4 activated into released seat (now 3/3)');

    // 11. Customer offline license file download endpoint
    const offlineDev = '5555555555555555555555555555555555555555555555555555555555555555';
    // First deactivate device 3 to have capacity
    await httpRequest('POST', '/api/v1/license/deactivate', {}, { productId: PRODUCT_ID, deviceId: device3 });
    const offlineRes = await httpRequest('POST', '/api/v1/license/offline/generate', {}, {
      serialKey: issuedSerial,
      deviceId: offlineDev
    });
    assert(offlineRes.status === 200 && offlineRes.body.fileName === 'proverb.ptlic', 'Offline .ptlic file generated for portal download');

    // 12. Refund Webhook -> Revokes license
    const refundPayload = {
      event: 'payment.refunded',
      event_id: `evt_refund_${Date.now()}`,
      payload: {
        payment: {
          entity: {
            id: paymentId,
            order_id: orderId
          }
        }
      }
    };
    const refundBody = JSON.stringify(refundPayload);
    const refundSig = crypto.createHmac('sha256', TEST_WEBHOOK_SECRET).update(refundBody).digest('hex');
    const refundRes = await httpRequest('POST', '/api/v1/license/webhook/razorpay', {
      'X-Razorpay-Signature': refundSig
    }, refundBody);
    assert(refundRes.status === 200 && refundRes.body.message.includes('revoked'), 'Refund webhook processed and license revoked');

    // 13. Revoked serial cannot activate anymore
    const postRefundAct = await httpRequest('POST', '/api/v1/license/serial/activate', {}, {
      productId: PRODUCT_ID,
      serialKey: issuedSerial,
      deviceId: '6666666666666666666666666666666666666666666666666666666666666666'
    });
    assert(postRefundAct.status === 400 && postRefundAct.body.error.includes('revoked'), 'Revoked serial blocked from activation');

  } catch (err) {
    console.error('[UNEXPECTED TEST ERROR]', err);
    failed++;
  }

  console.log('===============================================================');
  console.log(` RESULTS: Passed: ${passed} | Failed: ${failed}`);
  console.log('===============================================================');

  if (failed === 0) {
    console.log('[SUCCESS] ALL 13 PRODUCTION LICENSING TESTS PASSED!');
    process.exit(0);
  } else {
    console.error('[ERROR] Some tests failed.');
    process.exit(1);
  }
}

runTests();
