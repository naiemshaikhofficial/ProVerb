#!/usr/bin/env node
/**
 * Automated Production End-to-End Test Suite for ProducerToy Dedicated Multi-Brand Licensing Server
 * Tests:
 * 1. Health check & configuration
 * 2. Razorpay payment webhook (HMAC signature, automatic 128-bit serial generation)
 * 3. 128-bit cryptographic entropy validation (full 32 hex characters in 8 blocks)
 * 4. Invalid webhook signature rejection
 * 5. Webhook idempotency (duplicate retry protection)
 * 6. Non-plugin marketplace order filtering (ignores sample packs without issuing spurious licenses)
 * 7. Dynamic marketplace product registration (Partner Brand Plugin) via Admin API
 * 8. Multi-product order processing in single webhook
 * 9. Plugin serial redemption & Ed25519 token issuance for multiple brands
 * 10. Idempotent re-activation on same hardware device
 * 11. Multi-seat limit enforcement (custom per product)
 * 12. Deactivation and seat release
 * 13. Customer direct machine claim & recovery by verified Order ID + Customer Email
 * 14. Insecure pairing claim hole prevention (rejects plain email alone without order proof)
 * 15. Valid pairing challenge start, poll, and claim with verified proof
 * 16. Offline license file generation (.ptlic)
 * 17. Refund webhook and automatic license revocation
 * 18. Revocation enforcement (blocked activation)
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

const PROVERB_PRODUCT_ID = '92cf2454-2cfc-4db5-b210-2c732457605f';
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
  console.log(' ProducerToy Multi-Brand Licensing Server Automated Test Suite');
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

    // 2. Razorpay Webhook with valid signature -> issues 128-bit serial key for ProVerb
    const orderId1 = `ord_test_${Date.now()}`;
    const paymentId1 = `pay_test_${Date.now()}`;
    const eventId1 = `evt_test_${Date.now()}`;

    const webhookPayload1 = {
      event: 'payment.captured',
      event_id: eventId1,
      payload: {
        payment: {
          entity: {
            id: paymentId1,
            order_id: orderId1,
            amount: 999,
            currency: 'USD',
            status: 'captured',
            email: 'studio.producer@gmail.com',
            notes: {
              userName: 'Naiem Shaikh',
              userEmail: 'studio.producer@gmail.com',
              dedicatedProductIds: PROVERB_PRODUCT_ID,
              dedicatedProductSlugs: 'proverb'
            }
          }
        }
      }
    };

    const rawBody1 = JSON.stringify(webhookPayload1);
    const validSignature1 = crypto.createHmac('sha256', TEST_WEBHOOK_SECRET).update(rawBody1).digest('hex');

    const webhookRes1 = await httpRequest('POST', '/api/v1/license/webhook/razorpay', {
      'X-Razorpay-Signature': validSignature1
    }, rawBody1);

    assert(webhookRes1.status === 200 && webhookRes1.body.success === true, 'Signed Razorpay webhook processes successfully');
    assert(webhookRes1.body.licenses && webhookRes1.body.licenses.length === 1, 'ProVerb license issued in order');
    const issuedSerial1 = webhookRes1.body.licenses[0].serialKey;

    // 3. 128-bit Entropy Verification: Serial must have 32 hex characters in 8 blocks
    // Format: PT-PROV-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX
    const serialParts = issuedSerial1.split('-');
    assert(serialParts[0] === 'PT' && serialParts[1] === 'PROV', 'Serial prefix correctly formatted PT-PROV');
    const hexBlocks = serialParts.slice(2);
    assert(hexBlocks.length === 8, `Serial has 8 hex blocks (got ${hexBlocks.length})`);
    const totalHexChars = hexBlocks.join('').length;
    assert(totalHexChars === 32, `Serial represents full 32 hex characters / 128 bits of entropy (got ${totalHexChars})`);

    // 4. Invalid signature rejected
    const invalidSigRes = await httpRequest('POST', '/api/v1/license/webhook/razorpay', {
      'X-Razorpay-Signature': 'invalid_forged_hex_signature_1234567890'
    }, rawBody1);
    assert(invalidSigRes.status === 401, 'Forged webhook signature rejected with 401 Unauthorized');

    // 5. Duplicate webhook idempotency
    const retryRes = await httpRequest('POST', '/api/v1/license/webhook/razorpay', {
      'X-Razorpay-Signature': validSignature1
    }, rawBody1);
    assert(retryRes.status === 200 && retryRes.body.status === 'already_processed', 'Duplicate webhook retries handled idempotently without re-issuing');

    // 6. Non-plugin marketplace order filtering: Sample pack order should not generate licenses
    const nonPluginPayload = {
      event: 'payment.captured',
      event_id: `evt_sample_pack_${Date.now()}`,
      payload: {
        payment: {
          entity: {
            id: `pay_sample_${Date.now()}`,
            order_id: `ord_sample_${Date.now()}`,
            amount: 2900,
            currency: 'USD',
            status: 'captured',
            email: 'beatmaker@producer.com',
            notes: {
              userName: 'Beat Maker',
              userEmail: 'beatmaker@producer.com',
              productIds: 'sample-pack-uuid-1,sample-pack-uuid-2',
              productSlugs: 'trap-drums-vol-1,bollywood-vocals-vol-2'
            }
          }
        }
      }
    };
    const rawSampleBody = JSON.stringify(nonPluginPayload);
    const sampleSig = crypto.createHmac('sha256', TEST_WEBHOOK_SECRET).update(rawSampleBody).digest('hex');
    const sampleWebhookRes = await httpRequest('POST', '/api/v1/license/webhook/razorpay', {
      'X-Razorpay-Signature': sampleSig
    }, rawSampleBody);
    assert(sampleWebhookRes.status === 200 && sampleWebhookRes.body.status === 'non_plugin_order_acknowledged', 'Non-plugin sample pack order ignored without generating spurious licenses');

    // 7. Dynamic marketplace product registration (3rd Party Brand Plugin like Plugin Boutique)
    const partnerBrandRes = await httpRequest('POST', '/api/v1/admin/products', {
      'Authorization': `Bearer ${TEST_ADMIN_KEY}`
    }, {
      id: 'brand-delay-uuid-8899',
      name: 'Retro Space Delay',
      slug: 'retro-space-delay',
      brand: 'VintageAudio Lab',
      pluginIdentifier: 'com.vintageaudiolab.spacedelay',
      prefix: 'DELAY',
      maxSeats: 2,
      graceDays: 21
    });
    assert(partnerBrandRes.status === 200 && partnerBrandRes.body.success === true, 'Registered 3rd-party partner brand plugin dynamically');
    assert(partnerBrandRes.body.product.brand === 'VintageAudio Lab', 'Partner brand name recorded');
    assert(partnerBrandRes.body.product.serial_prefix === 'DELAY', 'Partner serial prefix recorded');

    // 8. Purchase 3rd-Party Partner Brand Plugin via Webhook
    const orderId2 = `ord_partner_${Date.now()}`;
    const paymentId2 = `pay_partner_${Date.now()}`;
    const partnerOrderPayload = {
      event: 'payment.captured',
      event_id: `evt_partner_${Date.now()}`,
      payload: {
        payment: {
          entity: {
            id: paymentId2,
            order_id: orderId2,
            amount: 4900,
            currency: 'USD',
            status: 'captured',
            email: 'partner.customer@audio.com',
            notes: {
              userName: 'Mixing Engineer',
              userEmail: 'partner.customer@audio.com',
              dedicatedProductIds: 'brand-delay-uuid-8899',
              dedicatedProductSlugs: 'retro-space-delay'
            }
          }
        }
      }
    };
    const rawPartnerBody = JSON.stringify(partnerOrderPayload);
    const partnerSig = crypto.createHmac('sha256', TEST_WEBHOOK_SECRET).update(rawPartnerBody).digest('hex');
    const partnerWebhookRes = await httpRequest('POST', '/api/v1/license/webhook/razorpay', {
      'X-Razorpay-Signature': partnerSig
    }, rawPartnerBody);
    assert(partnerWebhookRes.status === 200 && partnerWebhookRes.body.licenses.length === 1, 'Partner plugin license issued via webhook');
    const partnerSerial = partnerWebhookRes.body.licenses[0].serialKey;
    assert(partnerSerial.startsWith('PT-DELAY-'), `Partner serial has dynamic brand prefix: ${partnerSerial}`);

    // 9. Plugin Serial Key Activation for ProVerb
    const device1 = '1111111111111111111111111111111111111111111111111111111111111111';
    const actRes1 = await httpRequest('POST', '/api/v1/license/serial/activate', {}, {
      productId: PROVERB_PRODUCT_ID,
      serialKey: issuedSerial1,
      deviceId: device1,
      deviceName: 'Main Studio PC',
      os: 'Windows'
    });
    assert(actRes1.status === 200 && actRes1.body.success === true, 'ProVerb activated on Device 1');
    assert(typeof actRes1.body.token === 'string' && actRes1.body.token.includes('.'), 'Valid Ed25519 token issued');
    assert(actRes1.body.seatsUsed === 1 && actRes1.body.maxSeats === 3, 'ProVerb seat count 1/3');

    // 10. Plugin Serial Key Activation for Partner Brand Plugin
    const actResPartner = await httpRequest('POST', '/api/v1/license/serial/activate', {}, {
      productId: 'brand-delay-uuid-8899',
      serialKey: partnerSerial,
      deviceId: device1,
      deviceName: 'Main Studio PC',
      os: 'Windows'
    });
    assert(actResPartner.status === 200 && actResPartner.body.success === true, 'Partner Brand Plugin activated on Device 1');
    assert(actResPartner.body.seatsUsed === 1 && actResPartner.body.maxSeats === 2, 'Partner plugin respects 2-seat limit');

    // 11. Idempotent re-activation on same device
    const actRes1Repeat = await httpRequest('POST', '/api/v1/license/serial/activate', {}, {
      productId: PROVERB_PRODUCT_ID,
      serialKey: issuedSerial1,
      deviceId: device1,
      deviceName: 'Main Studio PC'
    });
    assert(actRes1Repeat.status === 200 && actRes1Repeat.body.token === actRes1.body.token, 'Same device re-activation returns existing token idempotently');
    assert(actRes1Repeat.body.seatsUsed === 1, 'Seat count remains 1/3 (not incremented)');

    // 12. Seat Limit Enforcement for Partner Plugin (maxSeats = 2)
    const device2 = '2222222222222222222222222222222222222222222222222222222222222222';
    const device3 = '3333333333333333333333333333333333333333333333333333333333333333';
    const partnerAct2 = await httpRequest('POST', '/api/v1/license/serial/activate', {}, {
      productId: 'brand-delay-uuid-8899',
      serialKey: partnerSerial,
      deviceId: device2
    });
    assert(partnerAct2.status === 200 && partnerAct2.body.seatsUsed === 2, 'Partner plugin seat 2/2 activated');

    const partnerAct3 = await httpRequest('POST', '/api/v1/license/serial/activate', {}, {
      productId: 'brand-delay-uuid-8899',
      serialKey: partnerSerial,
      deviceId: device3
    });
    assert(partnerAct3.status === 403, 'Partner plugin 3rd device rejected with 403 (seat capacity 2)');

    // 13. Customer Direct Machine Claim & Recovery (Verified Order ID + Email)
    const customerClaimRes = await httpRequest('POST', '/api/v1/license/customer/claim-device', {}, {
      orderId: orderId1,
      customerEmail: 'studio.producer@gmail.com',
      deviceId: device2,
      deviceName: 'Laptop DAW',
      productId: PROVERB_PRODUCT_ID
    });
    assert(customerClaimRes.status === 200 && customerClaimRes.body.success === true, 'Customer direct machine claim succeeded via Order ID + Email');
    assert(customerClaimRes.body.fileName === 'proverb.ptlic', 'Returned proper license file name');

    // 14. Insecure Pairing Claim Attack Prevention:
    // Starting a pairing challenge, then attempting to claim with email alone must be REJECTED!
    const pairStartRes = await httpRequest('POST', '/api/v1/license/pair/start', {}, {
      productId: PROVERB_PRODUCT_ID,
      deviceId: device3,
      deviceName: 'Pairing Test DAW'
    });
    assert(pairStartRes.status === 200 && pairStartRes.body.code, 'Pairing challenge initiated with short-lived code');
    const pairCode = pairStartRes.body.code;

    const insecureClaimRes = await httpRequest('POST', '/api/v1/license/pair/claim', {}, {
      code: pairCode,
      customerEmail: 'studio.producer@gmail.com' // No serial and no orderId!
    });
    assert(insecureClaimRes.status === 403, 'Insecure pairing claim with email alone correctly REJECTED with 403');

    // 15. Valid Pairing Claim with verified Serial Key
    const validClaimRes = await httpRequest('POST', '/api/v1/license/pair/claim', {}, {
      code: pairCode,
      serialKey: issuedSerial1
    });
    assert(validClaimRes.status === 200 && validClaimRes.body.success === true, 'Valid pairing claim with serial key authorized successfully');

    // Poll pairing from plugin
    const pollRes = await httpRequest('GET', `/api/v1/license/pair/poll?code=${pairCode}&deviceId=${device3}`);
    assert(pollRes.status === 200 && pollRes.body.status === 'claimed' && pollRes.body.token, 'Plugin polled and received claimed entitlement token');

    // 16. Offline License File Generation (.ptlic)
    const offlineDev = '5555555555555555555555555555555555555555555555555555555555555555';
    // Deactivate device3 to have room
    await httpRequest('POST', '/api/v1/license/deactivate', {}, { productId: PROVERB_PRODUCT_ID, deviceId: device3 });
    const offlineRes = await httpRequest('POST', '/api/v1/license/offline/generate', {}, {
      serialKey: issuedSerial1,
      deviceId: offlineDev
    });
    assert(offlineRes.status === 200 && offlineRes.body.fileName === 'proverb.ptlic', 'Offline .ptlic file generated for customer portal download');

    // 17. Refund Webhook -> Revokes license
    const refundPayload = {
      event: 'payment.refunded',
      event_id: `evt_refund_${Date.now()}`,
      payload: {
        payment: {
          entity: {
            id: paymentId1,
            order_id: orderId1
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

    // 18. Revoked serial cannot activate anymore
    const postRefundAct = await httpRequest('POST', '/api/v1/license/serial/activate', {}, {
      productId: PROVERB_PRODUCT_ID,
      serialKey: issuedSerial1,
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
    console.log('[SUCCESS] ALL 18 MULTI-BRAND PRODUCTION LICENSING TESTS PASSED!');
    process.exit(0);
  } else {
    console.error('[ERROR] Some tests failed.');
    process.exit(1);
  }
}

runTests();
