#!/usr/bin/env node
/**
 * ProducerToy Dedicated Standalone Licensing Server
 * Multi-Brand Plugin Marketplace Engine (Zero Vercel / Zero Supabase Dependencies)
 * 
 * Supports first-party ProducerToy plugins (ProVerb, etc.) and third-party marketplace partner brands
 * (Plugin Boutique style) dynamically without hardcoding product IDs or brand names.
 * 
 * Features:
 * - SQLite transactional persistence (WAL mode)
 * - Ed25519 asymmetric token signing (device-hardware bound)
 * - True 128-bit entropy serial key issuance (full 32 hex characters / 16 bytes)
 * - Dynamic product catalog (supports multiple brands, custom seats, custom serial prefixes)
 * - Razorpay and Generic signed payment webhooks with timing-safe HMAC validation
 * - Unrelated non-plugin order filtering (only issues licenses for verified plugin products)
 * - Customer direct machine claim & recovery by verified Order ID + Customer Email
 * - Replay & brute-force defense (rate limiting, atomic challenges, timing-safe crypto)
 * - No plaintext serial logging
 */

import http from 'node:http';
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { DatabaseSync } from 'node:sqlite';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// ==============================================================================
// 1. Production Configuration & Strict Validation
// ==============================================================================
const PORT = parseInt(process.env.PORT || '4000', 10);
const HOST = process.env.HOST || '0.0.0.0';
const DATA_DIR = process.env.DATA_DIR || path.join(__dirname, 'data');
const DB_FILE = path.join(DATA_DIR, 'licensing.db');
const TRUST_PROXY = process.env.TRUST_PROXY === 'true';

// Default / Legacy ProVerb Constants (Backward Compatibility)
const DEFAULT_PRODUCT_ID = '92cf2454-2cfc-4db5-b210-2c732457605f';
const DEFAULT_PRODUCT_SLUG = 'proverb';
const DEFAULT_PRODUCT_NAME = 'ProVerb';
const DEFAULT_PLUGIN_ID = 'com.producertoy.proverb';
const KEY_ID = 'proverb-ed25519-2026-10-v2';
const DEFAULT_MAX_SEATS = 3;
const DEFAULT_OFFLINE_GRACE_DAYS = 14;

// Secrets Validation: FAIL FAST if secrets are missing or insecure
const SALT_SECRET = process.env.LICENSING_SALT_SECRET;
if (!SALT_SECRET || SALT_SECRET.length < 32) {
  console.error('[FATAL ERROR] LICENSING_SALT_SECRET must be set and contain at least 32 characters of high entropy.');
  console.error('Do not start licensing server with missing or weak salt secrets.');
  process.exit(1);
}

const ADMIN_API_KEY = process.env.LICENSING_ADMIN_KEY || process.env.ADMIN_SECRET;
if (!ADMIN_API_KEY || ADMIN_API_KEY.length < 16) {
  console.error('[FATAL ERROR] LICENSING_ADMIN_KEY / ADMIN_SECRET must be configured with at least 16 characters.');
  process.exit(1);
}

const RAZORPAY_WEBHOOK_SECRET = process.env.RAZORPAY_WEBHOOK_SECRET || process.env.WEBHOOK_SECRET || '';
const GENERIC_WEBHOOK_SECRET = process.env.GENERIC_WEBHOOK_SECRET || RAZORPAY_WEBHOOK_SECRET;

// Load Ed25519 Private Key
function loadSigningKey() {
  let pem = process.env.PROVERB_ED25519_PRIVATE_KEY_PEM || process.env.ED25519_PRIVATE_KEY;
  if (!pem && process.env.PROVERB_SIGNING_KEY) {
    if (fs.existsSync(process.env.PROVERB_SIGNING_KEY)) {
      pem = fs.readFileSync(process.env.PROVERB_SIGNING_KEY, 'utf8');
    }
  }
  if (!pem && process.env.LOCALAPPDATA) {
    const localKey = path.join(process.env.LOCALAPPDATA, 'ProducerToy', 'Licensing', 'ProVerb', 'proverb-ed25519-private.pem');
    if (fs.existsSync(localKey)) {
      pem = fs.readFileSync(localKey, 'utf8');
    }
  }

  if (!pem) {
    console.error('[FATAL ERROR] Ed25519 private signing key not found!');
    console.error('Set PROVERB_SIGNING_KEY or PROVERB_ED25519_PRIVATE_KEY_PEM environment variable.');
    process.exit(1);
  }

  try {
    const key = crypto.createPrivateKey(pem);
    if (key.asymmetricKeyType !== 'ed25519') {
      throw new Error(`Key type is ${key.asymmetricKeyType}, expected ed25519.`);
    }
    return key;
  } catch (err) {
    console.error('[FATAL ERROR] Invalid Ed25519 private key:', err.message);
    process.exit(1);
  }
}

const signingKey = loadSigningKey();

// ==============================================================================
// 2. Transactional SQLite Database Setup (Multi-Brand & Dynamic Products)
// ==============================================================================
if (!fs.existsSync(DATA_DIR)) {
  fs.mkdirSync(DATA_DIR, { recursive: true });
}

const db = new DatabaseSync(DB_FILE);

// Enable WAL mode & foreign keys for high-performance ACID concurrency
db.exec(`
  PRAGMA journal_mode = WAL;
  PRAGMA synchronous = NORMAL;
  PRAGMA foreign_keys = ON;

  -- 1. Products catalog (Dynamic: supports multiple brands and plugins)
  CREATE TABLE IF NOT EXISTS products (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    slug TEXT NOT NULL UNIQUE,
    brand TEXT NOT NULL DEFAULT 'Producer Toy',
    plugin_identifier TEXT NOT NULL UNIQUE,
    serial_prefix TEXT NOT NULL DEFAULT 'PROV',
    max_activation_seats INTEGER NOT NULL DEFAULT 3,
    offline_grace_days INTEGER NOT NULL DEFAULT 14,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
  );

  -- 2. Purchases & Licenses
  CREATE TABLE IF NOT EXISTS purchases (
    id TEXT PRIMARY KEY,
    serial_hash TEXT UNIQUE NOT NULL,
    order_id TEXT NOT NULL,
    event_id TEXT,
    payment_id TEXT,
    product_id TEXT NOT NULL REFERENCES products(id),
    plugin_identifier TEXT NOT NULL,
    customer_email TEXT NOT NULL,
    customer_name TEXT,
    max_activation_seats INTEGER NOT NULL DEFAULT 3,
    status TEXT NOT NULL DEFAULT 'active' CHECK(status IN ('active', 'revoked', 'refunded')),
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
  );

  -- 3. Device Activations
  CREATE TABLE IF NOT EXISTS device_activations (
    id TEXT PRIMARY KEY,
    purchase_id TEXT REFERENCES purchases(id) ON DELETE CASCADE,
    serial_hash TEXT NOT NULL REFERENCES purchases(serial_hash) ON DELETE CASCADE,
    product_id TEXT NOT NULL REFERENCES products(id),
    customer_email TEXT NOT NULL,
    device_id_hash TEXT NOT NULL,
    device_name TEXT NOT NULL DEFAULT 'Audio Workstation',
    os TEXT DEFAULT 'Windows',
    token TEXT NOT NULL,
    is_active INTEGER NOT NULL DEFAULT 1,
    activated_at TEXT NOT NULL DEFAULT (datetime('now')),
    last_seen_at TEXT NOT NULL DEFAULT (datetime('now')),
    revoked_at TEXT,
    metadata TEXT DEFAULT '{}',
    CONSTRAINT uq_active_device UNIQUE (serial_hash, device_id_hash)
  );

  -- 4. Activation Challenges (Pairing Codes)
  CREATE TABLE IF NOT EXISTS activation_challenges (
    id TEXT PRIMARY KEY,
    challenge_code_hash TEXT UNIQUE NOT NULL,
    code TEXT NOT NULL,
    product_id TEXT NOT NULL REFERENCES products(id),
    device_id_hash TEXT NOT NULL,
    device_name TEXT NOT NULL DEFAULT 'Audio Workstation',
    os TEXT,
    status TEXT NOT NULL DEFAULT 'pending' CHECK(status IN ('pending', 'claimed', 'expired', 'rejected', 'burned')),
    customer_email TEXT,
    entitlement_token TEXT,
    expires_at TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    claimed_at TEXT
  );

  -- 5. Webhook Idempotency Store
  CREATE TABLE IF NOT EXISTS idempotency_keys (
    event_id TEXT PRIMARY KEY,
    provider TEXT NOT NULL,
    processed_at TEXT NOT NULL DEFAULT (datetime('now')),
    response_json TEXT
  );

  -- 6. Audit Trail
  CREATE TABLE IF NOT EXISTS audit_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    event_type TEXT NOT NULL,
    reference_id TEXT,
    ip_address TEXT,
    meta_json TEXT,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
  );

  -- Indexes
  CREATE INDEX IF NOT EXISTS idx_purchases_email ON purchases(customer_email);
  CREATE INDEX IF NOT EXISTS idx_purchases_order ON purchases(order_id);
  CREATE INDEX IF NOT EXISTS idx_purchases_prod ON purchases(product_id);
  CREATE INDEX IF NOT EXISTS idx_device_act_hash ON device_activations(device_id_hash);
  CREATE INDEX IF NOT EXISTS idx_device_act_serial ON device_activations(serial_hash, is_active);
  CREATE INDEX IF NOT EXISTS idx_challenges_hash ON activation_challenges(challenge_code_hash, status);
`);

// Migration for existing databases: ensure brand and serial_prefix exist
try {
  db.exec("ALTER TABLE products ADD COLUMN brand TEXT DEFAULT 'Producer Toy'");
} catch {}
try {
  db.exec("ALTER TABLE products ADD COLUMN serial_prefix TEXT DEFAULT 'PROV'");
} catch {}

// Seed default ProVerb product (Producer Toy Flagship Reverb)
db.exec(`
  INSERT INTO products (id, name, slug, brand, plugin_identifier, serial_prefix, max_activation_seats, offline_grace_days)
  VALUES ('${DEFAULT_PRODUCT_ID}', '${DEFAULT_PRODUCT_NAME}', '${DEFAULT_PRODUCT_SLUG}', 'Producer Toy', '${DEFAULT_PLUGIN_ID}', 'PROV', ${DEFAULT_MAX_SEATS}, ${DEFAULT_OFFLINE_GRACE_DAYS})
  ON CONFLICT(id) DO UPDATE SET
    name = excluded.name,
    slug = excluded.slug,
    brand = excluded.brand,
    plugin_identifier = excluded.plugin_identifier,
    serial_prefix = excluded.serial_prefix,
    max_activation_seats = excluded.max_activation_seats,
    offline_grace_days = excluded.offline_grace_days;
`);

// Audit Logger
function audit(eventType, referenceId, ipAddress, meta = {}) {
  try {
    const stmt = db.prepare(`
      INSERT INTO audit_log (event_type, reference_id, ip_address, meta_json, created_at)
      VALUES (?, ?, ?, ?, datetime('now'))
    `);
    stmt.run(eventType, referenceId || null, ipAddress || 'unknown', JSON.stringify(meta));
  } catch (err) {
    console.error('[AUDIT LOG ERROR]', err.message);
  }
}

// Dynamic Product Catalog Resolvers
function resolveProduct(query) {
  if (!query) return null;
  const clean = String(query).trim().toLowerCase();
  return db.prepare(`
    SELECT * FROM products
    WHERE LOWER(id) = ?
       OR LOWER(slug) = ?
       OR LOWER(plugin_identifier) = ?
       OR LOWER(serial_prefix) = ?
       OR LOWER(name) = ?
    LIMIT 1
  `).get(clean, clean, clean, clean, clean);
}

function listAllProducts() {
  return db.prepare('SELECT * FROM products ORDER BY name ASC').all();
}

function getOrRegisterProduct({ id, name, slug, brand, pluginIdentifier, prefix, seats, graceDays }) {
  let prod = (id ? resolveProduct(id) : null) || (slug ? resolveProduct(slug) : null) || (pluginIdentifier ? resolveProduct(pluginIdentifier) : null);
  if (prod) return prod;

  const prodId = id || crypto.randomUUID();
  const prodSlug = (slug || name || 'plugin').toLowerCase().replace(/[^a-z0-9_-]/g, '-');
  const prodName = name || 'Audio Plugin';
  const prodBrand = brand || 'Producer Toy';
  const prodPluginId = pluginIdentifier || `com.${prodBrand.toLowerCase().replace(/[^a-z0-9]/g, '')}.${prodSlug}`;
  const prodPrefix = (prefix || prodSlug.replace(/[^a-z0-9]/g, '').slice(0, 5) || 'VST').toUpperCase();
  const maxSeats = parseInt(seats || '3', 10);
  const offlineGrace = parseInt(graceDays || '14', 10);

  db.prepare(`
    INSERT INTO products (id, name, slug, brand, plugin_identifier, serial_prefix, max_activation_seats, offline_grace_days)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    ON CONFLICT(id) DO UPDATE SET
      name = excluded.name,
      slug = excluded.slug,
      brand = excluded.brand,
      plugin_identifier = excluded.plugin_identifier,
      serial_prefix = excluded.serial_prefix
  `).run(prodId, prodName, prodSlug, prodBrand, prodPluginId, prodPrefix, maxSeats, offlineGrace);

  return resolveProduct(prodId);
}

// Keyed Hash for Serial Keys (Never store plaintext serials)
function hashSerial(serialKey) {
  const normalized = serialKey.trim().toUpperCase().replace(/[^A-Z0-9]/g, '');
  return crypto.createHmac('sha256', SALT_SECRET).update(normalized).digest('hex');
}

/**
 * 128-Bit Entropy Serial Key Generator (Format: PT-<PREFIX>-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX-XXXX)
 * Uses all 16 random bytes (32 hex characters) for full 128-bit cryptographic strength.
 */
function generateCryptographicSerial(prefix = 'PROV') {
  const cleanPrefix = String(prefix || 'VST').toUpperCase().replace(/[^A-Z0-9]/g, '').slice(0, 6) || 'VST';
  const bytes = crypto.randomBytes(16); // 16 bytes = 128 bits of true entropy
  const hex = bytes.toString('hex').toUpperCase(); // 32 hex characters
  return `PT-${cleanPrefix}-${hex.slice(0, 4)}-${hex.slice(4, 8)}-${hex.slice(8, 12)}-${hex.slice(12, 16)}-${hex.slice(16, 20)}-${hex.slice(20, 24)}-${hex.slice(24, 28)}-${hex.slice(28, 32)}`;
}

// Mask Email for privacy in client UI & logs
function maskEmail(email) {
  if (!email || !email.includes('@')) return 'customer@producertoy.com';
  const parts = email.split('@');
  const local = parts[0];
  const domain = parts[1];
  const visible = local.length <= 2 ? local[0] : local[0] + '***' + local[local.length - 1];
  return visible + '@' + domain;
}

// Issue signed Ed25519 token (Dynamically bound to product and hardware device)
function issueEntitlementToken(product, deviceId, userName, userEmail, activationId = crypto.randomUUID()) {
  const now = Math.floor(Date.now() / 1000);
  const cleanDevId = deviceId.trim().toLowerCase().replace(/[^0-9a-f]/g, '');

  const payload = {
    v: 2,
    iss: 'producertoy.com',
    aud: product.plugin_identifier,
    pid: product.id,
    product_name: product.name,
    brand: product.brand || 'Producer Toy',
    plugin_id: product.plugin_identifier,
    kid: KEY_ID,
    status: 'active',
    aid: activationId,
    dev_hash: cleanDevId,
    user_name: userName || 'Licensed User',
    user_email_masked: maskEmail(userEmail),
    tier: 'perpetual',
    iat: now,
    nbf: now,
    exp: 0 // Perpetual license
  };

  const encodedPayload = Buffer.from(JSON.stringify(payload), 'utf8').toString('base64url');
  const signature = crypto.sign(null, Buffer.from(encodedPayload, 'ascii'), signingKey).toString('base64url');
  return {
    token: `${encodedPayload}.${signature}`,
    activationId,
    payload
  };
}

// Rate Limiting (In-Memory sliding window)
const rateLimits = new Map();
function checkRateLimit(ip, maxAttempts = 20, windowMs = 15 * 60 * 1000) {
  const now = Date.now();
  const entry = rateLimits.get(ip) || { count: 0, windowStart: now };
  if (now - entry.windowStart > windowMs) {
    entry.count = 1;
    entry.windowStart = now;
  } else {
    entry.count += 1;
  }
  rateLimits.set(ip, entry);
  return entry.count <= maxAttempts;
}

// Background cleanup of expired pairing sessions
setInterval(() => {
  try {
    const stmt = db.prepare(`
      UPDATE activation_challenges
      SET status = 'expired'
      WHERE status = 'pending' AND datetime('now') > datetime(expires_at)
    `);
    stmt.run();
  } catch (e) {
    console.error('Cleanup error:', e.message);
  }
}, 60000);

// Helper to parse JSON body
function parseJsonBody(req) {
  return new Promise((resolve, reject) => {
    let body = '';
    req.on('data', chunk => {
      body += chunk;
      if (body.length > 262144) { // 256KB max
        req.destroy();
        reject(new Error('Payload too large'));
      }
    });
    req.on('end', () => {
      try {
        resolve({ json: body ? JSON.parse(body) : {}, rawBody: body });
      } catch (err) {
        reject(new Error('Invalid JSON format'));
      }
    });
    req.on('error', reject);
  });
}

function sendJson(res, statusCode, data) {
  res.writeHead(statusCode, {
    'Content-Type': 'application/json',
    'Cache-Control': 'no-store, no-cache, must-revalidate',
    'X-Content-Type-Options': 'nosniff',
    'X-Frame-Options': 'DENY'
  });
  res.end(JSON.stringify(data));
}

function getClientIp(req) {
  if (TRUST_PROXY) {
    const xff = req.headers['x-forwarded-for'];
    if (xff) {
      return xff.split(',')[0].trim();
    }
  }
  return req.socket.remoteAddress || 'unknown';
}

// ==============================================================================
// 3. HTTP Server Endpoints
// ==============================================================================
const server = http.createServer(async (req, res) => {
  const ip = getClientIp(req);
  const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
  const pathname = url.pathname;

  // CORS
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization, Accept, X-Razorpay-Signature, X-Webhook-Signature');

  if (req.method === 'OPTIONS') {
    res.writeHead(204);
    res.end();
    return;
  }

  try {
    // --------------------------------------------------------------------------
    // 1. Health & Readiness Probe
    // --------------------------------------------------------------------------
    if (pathname === '/health' || pathname === '/api/v1/health') {
      let dbOk = false;
      let productCount = 0;
      try {
        const row = db.prepare('SELECT COUNT(*) as count FROM products').get();
        productCount = row.count;
        dbOk = productCount > 0;
      } catch (e) {
        dbOk = false;
      }

      sendJson(res, 200, {
        status: dbOk && !!signingKey ? 'ready' : 'degraded',
        service: 'ProducerToy Multi-Brand Standalone Licensing Server',
        version: '2.1.0',
        signingKeyLoaded: !!signingKey,
        databaseType: 'sqlite_transactional_wal',
        databaseHealthy: dbOk,
        registeredProductsCount: productCount,
        paymentWebhookConfigured: !!RAZORPAY_WEBHOOK_SECRET,
        keyId: KEY_ID
      });
      return;
    }

    // --------------------------------------------------------------------------
    // 2. Direct Signed Razorpay Payment Webhook (Automatic Serial Issuance)
    // --------------------------------------------------------------------------
    if (pathname === '/api/v1/license/webhook/razorpay' && req.method === 'POST') {
      const { json: payload, rawBody } = await parseJsonBody(req);
      const signature = req.headers['x-razorpay-signature'];

      if (!RAZORPAY_WEBHOOK_SECRET) {
        sendJson(res, 503, { success: false, error: 'Razorpay webhook secret not configured on server.' });
        return;
      }

      if (!signature) {
        audit('webhook_missing_signature', null, ip);
        sendJson(res, 401, { success: false, error: 'Missing X-Razorpay-Signature header.' });
        return;
      }

      // Timing-safe HMAC-SHA256 signature verification
      const expectedSignature = crypto
        .createHmac('sha256', RAZORPAY_WEBHOOK_SECRET)
        .update(rawBody)
        .digest('hex');

      const sigBuffer = Buffer.from(signature, 'utf8');
      const expectedBuffer = Buffer.from(expectedSignature, 'utf8');

      const isSignatureValid =
        sigBuffer.length === expectedBuffer.length &&
        crypto.timingSafeEqual(sigBuffer, expectedBuffer);

      if (!isSignatureValid) {
        audit('webhook_invalid_signature', null, ip);
        sendJson(res, 401, { success: false, error: 'Invalid webhook signature.' });
        return;
      }

      const event = payload.event;
      const paymentEntity = payload.payload?.payment?.entity || {};
      const orderId = paymentEntity.order_id || payload.payload?.order?.entity?.id || `ord_${Date.now()}`;
      const eventId = payload.event_id || paymentEntity.id || `evt_${Date.now()}`;

      // Idempotency: Ignore already processed events safely
      const existingEvent = db.prepare('SELECT event_id FROM idempotency_keys WHERE event_id = ?').get(eventId);
      if (existingEvent) {
        audit('webhook_idempotent_retry', eventId, ip);
        sendJson(res, 200, { success: true, status: 'already_processed', eventId });
        return;
      }

      // Event A: Payment Captured / Order Paid -> Issue License Serial(s)
      if (event === 'payment.captured' || event === 'order.paid') {
        const customerEmail = (paymentEntity.notes?.userEmail || paymentEntity.email || '').trim().toLowerCase();
        const customerName = (paymentEntity.notes?.userName || paymentEntity.notes?.customer_name || 'ProducerToy Customer').trim();
        const paymentId = paymentEntity.id || '';

        if (!customerEmail) {
          sendJson(res, 400, { success: false, error: 'Customer email missing in payment payload.' });
          return;
        }

        // Dynamic multi-product identification from payment notes
        const dedicatedProductIds = (paymentEntity.notes?.dedicatedProductIds || '').split(',').filter(Boolean);
        const dedicatedProductSlugs = (paymentEntity.notes?.dedicatedProductSlugs || '').split(',').filter(Boolean);
        const allProductIds = (paymentEntity.notes?.productIds || '').split(',').filter(Boolean);
        const allProductSlugs = (paymentEntity.notes?.productSlugs || '').split(',').filter(Boolean);

        const targetProducts = [];
        const checkedKeys = new Set([...dedicatedProductIds, ...dedicatedProductSlugs, ...allProductIds, ...allProductSlugs]);

        for (const key of checkedKeys) {
          const p = resolveProduct(key);
          if (p && !targetProducts.some(tp => tp.id === p.id)) {
            targetProducts.push(p);
          }
        }

        // Fallback for legacy single-item ProVerb orders
        if (targetProducts.length === 0 && (paymentEntity.notes?.hasProVerb === 'true' || (paymentEntity.description && paymentEntity.description.toLowerCase().includes('proverb')))) {
          const pv = resolveProduct(DEFAULT_PRODUCT_ID);
          if (pv) targetProducts.push(pv);
        }

        // If no plugin products found in this order, acknowledge safely without issuing spurious licenses
        if (targetProducts.length === 0) {
          db.prepare("INSERT INTO idempotency_keys (event_id, provider, response_json, processed_at) VALUES (?, ?, ?, datetime('now'))")
            .run(eventId, 'razorpay', JSON.stringify({ status: 'ignored_non_plugin_order' }));
          sendJson(res, 200, { success: true, status: 'non_plugin_order_acknowledged', orderId });
          return;
        }

        const issuedLicenses = [];

        db.exec('BEGIN IMMEDIATE');
        try {
          const insertStmt = db.prepare(`
            INSERT INTO purchases (
              id, serial_hash, order_id, event_id, payment_id,
              product_id, plugin_identifier, customer_email, customer_name,
              max_activation_seats, status, created_at, updated_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'active', datetime('now'), datetime('now'))
          `);

          for (const product of targetProducts) {
            // Check if already purchased in this order
            const existingPurchase = db.prepare(`
              SELECT id, serial_hash FROM purchases 
              WHERE (payment_id = ? OR order_id = ?) AND product_id = ?
            `).get(paymentId, orderId, product.id);

            if (existingPurchase) {
              issuedLicenses.push({
                productId: product.id,
                productName: product.name,
                status: 'already_issued'
              });
              continue;
            }

            const serialKey = generateCryptographicSerial(product.serial_prefix);
            const serialHash = hashSerial(serialKey);
            const purchaseId = crypto.randomUUID();

            insertStmt.run(
              purchaseId, serialHash, orderId, eventId, paymentId,
              product.id, product.plugin_identifier, customerEmail, customerName, product.max_activation_seats
            );

            issuedLicenses.push({
              productId: product.id,
              productName: product.name,
              pluginIdentifier: product.plugin_identifier,
              serialKey,
              maxSeats: product.max_activation_seats
            });
          }

          db.prepare(`
            INSERT INTO idempotency_keys (event_id, provider, response_json, processed_at)
            VALUES (?, 'razorpay', ?, datetime('now'))
          `).run(eventId, JSON.stringify({ status: 'issued', count: issuedLicenses.length }));

          db.exec('COMMIT');
        } catch (dbErr) {
          db.exec('ROLLBACK');
          console.error('[WEBHOOK DB ERROR]', dbErr.message);
          sendJson(res, 500, { success: false, error: 'Database error storing license.' });
          return;
        }

        audit('serial_issued_via_webhook', orderId, ip, {
          itemCount: issuedLicenses.length,
          customerEmail: maskEmail(customerEmail)
        });

        // Safe logging: Never log plaintext serials in full production logs
        console.log(`[LICENSE ISSUED] Order: ${orderId} | Customer: ${maskEmail(customerEmail)} | Plugins: ${targetProducts.map(p => p.name).join(', ')}`);

        sendJson(res, 200, {
          success: true,
          message: 'License created and serial keys issued successfully.',
          orderId,
          licenses: issuedLicenses,
          customerEmail: maskEmail(customerEmail)
        });
        return;
      }

      // Event B: Refund / Chargeback -> Revoke License
      if (event === 'payment.refunded' || event === 'refund.created') {
        const paymentId = paymentEntity.id || '';
        db.exec('BEGIN IMMEDIATE');
        try {
          const purchases = db.prepare('SELECT id, serial_hash FROM purchases WHERE payment_id = ? OR order_id = ?').all(paymentId, orderId);
          for (const purchase of purchases) {
            db.prepare("UPDATE purchases SET status = 'refunded', updated_at = datetime('now') WHERE id = ?").run(purchase.id);
            db.prepare("UPDATE device_activations SET is_active = 0, revoked_at = datetime('now') WHERE serial_hash = ?").run(purchase.serial_hash);
          }
          db.prepare(`
            INSERT INTO idempotency_keys (event_id, provider, response_json, processed_at)
            VALUES (?, 'razorpay', ?, datetime('now'))
          `).run(eventId, JSON.stringify({ status: 'refund_processed', revokedCount: purchases.length }));

          db.exec('COMMIT');
        } catch (refErr) {
          db.exec('ROLLBACK');
          console.error('[REFUND DB ERROR]', refErr.message);
        }

        audit('license_revoked_refund', orderId, ip, { paymentId });
        sendJson(res, 200, { success: true, message: 'License(s) revoked due to refund.' });
        return;
      }

      // Other unhandled events acknowledge 200 OK
      sendJson(res, 200, { success: true, status: 'ignored_event', event });
      return;
    }

    // --------------------------------------------------------------------------
    // 3. Generic Payment Webhook (PayPal / Stripe / Shopify / Marketplace)
    // --------------------------------------------------------------------------
    if (pathname === '/api/v1/license/webhook/generic' && req.method === 'POST') {
      const { json: payload, rawBody } = await parseJsonBody(req);
      const signature = req.headers['x-webhook-signature'];

      if (GENERIC_WEBHOOK_SECRET) {
        if (!signature) {
          sendJson(res, 401, { success: false, error: 'Missing X-Webhook-Signature.' });
          return;
        }
        const expected = crypto.createHmac('sha256', GENERIC_WEBHOOK_SECRET).update(rawBody).digest('hex');
        const sigBuf = Buffer.from(signature, 'utf8');
        const expBuf = Buffer.from(expected, 'utf8');
        if (sigBuf.length !== expBuf.length || !crypto.timingSafeEqual(sigBuf, expBuf)) {
          sendJson(res, 401, { success: false, error: 'Invalid webhook signature.' });
          return;
        }
      }

      const orderId = payload.orderId || payload.order_id || `ord_${Date.now()}`;
      const eventId = payload.eventId || payload.id || `evt_${Date.now()}`;
      const customerEmail = (payload.email || payload.customer_email || '').trim().toLowerCase();
      const customerName = payload.name || payload.customer_name || 'Customer';

      if (!customerEmail) {
        sendJson(res, 400, { success: false, error: 'Customer email required.' });
        return;
      }

      const existing = db.prepare('SELECT event_id FROM idempotency_keys WHERE event_id = ?').get(eventId);
      if (existing) {
        sendJson(res, 200, { success: true, status: 'already_processed', eventId });
        return;
      }

      const product = resolveProduct(payload.productId || payload.product_id || payload.slug || payload.pluginId) || resolveProduct(DEFAULT_PRODUCT_ID);
      const serialKey = generateCryptographicSerial(product?.serial_prefix);
      const serialHash = hashSerial(serialKey);
      const purchaseId = crypto.randomUUID();

      db.exec('BEGIN IMMEDIATE');
      try {
        db.prepare(`
          INSERT INTO purchases (
            id, serial_hash, order_id, event_id, payment_id,
            product_id, plugin_identifier, customer_email, customer_name,
            max_activation_seats, status, created_at, updated_at
          ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'active', datetime('now'), datetime('now'))
        `).run(
          purchaseId, serialHash, orderId, eventId, payload.paymentId || '',
          product.id, product.plugin_identifier, customerEmail, customerName, product.max_activation_seats
        );

        db.prepare("INSERT INTO idempotency_keys (event_id, provider, response_json, processed_at) VALUES (?, ?, ?, datetime('now'))")
          .run(eventId, 'generic', JSON.stringify({ status: 'issued', purchaseId }));

        db.exec('COMMIT');
      } catch (err) {
        db.exec('ROLLBACK');
        sendJson(res, 500, { success: false, error: err.message });
        return;
      }

      sendJson(res, 200, { success: true, orderId, serialKey, customerEmail: maskEmail(customerEmail) });
      return;
    }

    // --------------------------------------------------------------------------
    // 4. Serial Key Activation (Plugin GUI / Standalone Activation)
    // --------------------------------------------------------------------------
    if (pathname === '/api/v1/license/serial/activate' && req.method === 'POST') {
      if (!checkRateLimit(ip, 20, 15 * 60 * 1000)) {
        audit('serial_rate_limited', null, ip);
        sendJson(res, 429, { success: false, error: 'Too many requests. Please try again in 15 minutes.' });
        return;
      }

      const { json: body } = await parseJsonBody(req);
      const { productId, serialKey, deviceId, deviceName, os } = body;

      if (!serialKey || typeof serialKey !== 'string') {
        sendJson(res, 400, { success: false, error: 'Missing serial key.' });
        return;
      }
      const cleanDevId = (deviceId || '').trim().toLowerCase().replace(/[^0-9a-f]/g, '');
      if (cleanDevId.length !== 64) {
        sendJson(res, 400, { success: false, error: 'Invalid hardware device ID format.' });
        return;
      }

      const keyHash = hashSerial(serialKey);

      let activationResult = null;
      db.exec('BEGIN IMMEDIATE');
      try {
        const purchase = db.prepare(`
          SELECT p.*, prod.name as prod_name, prod.slug as prod_slug, prod.brand as prod_brand, prod.plugin_identifier as prod_plugin_id
          FROM purchases p
          JOIN products prod ON p.product_id = prod.id
          WHERE p.serial_hash = ? AND p.status = 'active'
        `).get(keyHash);

        if (!purchase) {
          db.exec('ROLLBACK');
          audit('serial_invalid_attempt', null, ip, { deviceIdPrefix: cleanDevId.slice(0, 8) });
          sendJson(res, 400, { success: false, error: 'The serial key entered is invalid or has been revoked.' });
          return;
        }

        // Verify product matches if caller sent a specific productId
        if (productId) {
          const reqProd = resolveProduct(productId);
          if (reqProd && reqProd.id !== purchase.product_id) {
            db.exec('ROLLBACK');
            sendJson(res, 400, { success: false, error: 'This serial key belongs to a different product.' });
            return;
          }
        }

        const product = {
          id: purchase.product_id,
          name: purchase.prod_name,
          slug: purchase.prod_slug,
          brand: purchase.prod_brand,
          plugin_identifier: purchase.prod_plugin_id
        };

        // Check if this device is already activated (Idempotency)
        const existingActivation = db.prepare(`
          SELECT * FROM device_activations
          WHERE serial_hash = ? AND device_id_hash = ?
        `).get(keyHash, cleanDevId);

        if (existingActivation && existingActivation.is_active) {
          db.exec('COMMIT');
          const seatCount = db.prepare('SELECT COUNT(*) as count FROM device_activations WHERE serial_hash = ? AND is_active = 1').get(keyHash).count;
          audit('serial_idempotent_reissue', purchase.id, ip, { deviceIdPrefix: cleanDevId.slice(0, 8) });

          sendJson(res, 200, {
            success: true,
            token: existingActivation.token,
            seatsUsed: seatCount,
            maxSeats: purchase.max_activation_seats
          });
          return;
        }

        // Count current active seats
        const activeSeatRow = db.prepare('SELECT COUNT(*) as count FROM device_activations WHERE serial_hash = ? AND is_active = 1').get(keyHash);
        const currentSeats = activeSeatRow.count;

        if (currentSeats >= purchase.max_activation_seats) {
          db.exec('ROLLBACK');
          audit('serial_seat_limit_exceeded', purchase.id, ip, { currentSeats, maxSeats: purchase.max_activation_seats });
          sendJson(res, 403, {
            success: false,
            error: `Seat limit reached (${currentSeats}/${purchase.max_activation_seats}). Please deactivate an existing machine first.`
          });
          return;
        }

        // Issue new Ed25519 token & register device
        const activationId = crypto.randomUUID();
        const entitlement = issueEntitlementToken(product, cleanDevId, purchase.customer_name, purchase.customer_email, activationId);

        if (existingActivation) {
          db.prepare(`
            UPDATE device_activations
            SET is_active = 1, token = ?, device_name = ?, os = ?, last_seen_at = datetime('now'), revoked_at = NULL
            WHERE id = ?
          `).run(entitlement.token, deviceName || 'Studio Workstation', os || 'Windows', existingActivation.id);
        } else {
          db.prepare(`
            INSERT INTO device_activations (
              id, purchase_id, serial_hash, product_id, customer_email,
              device_id_hash, device_name, os, token, is_active,
              activated_at, last_seen_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 1, datetime('now'), datetime('now'))
          `).run(
            activationId, purchase.id, keyHash, product.id, purchase.customer_email,
            cleanDevId, (deviceName || 'Studio Workstation').slice(0, 64), (os || 'Windows').slice(0, 32),
            entitlement.token
          );
        }

        db.exec('COMMIT');
        activationResult = {
          token: entitlement.token,
          seatsUsed: currentSeats + 1,
          maxSeats: purchase.max_activation_seats
        };
      } catch (actErr) {
        db.exec('ROLLBACK');
        console.error('[ACTIVATION ERROR]', actErr.message);
        sendJson(res, 500, { success: false, error: 'Database error during device seat activation.' });
        return;
      }

      audit('serial_activated_success', keyHash, ip, {
        deviceIdPrefix: cleanDevId.slice(0, 8),
        seatsUsed: activationResult.seatsUsed
      });

      sendJson(res, 200, {
        success: true,
        token: activationResult.token,
        seatsUsed: activationResult.seatsUsed,
        maxSeats: activationResult.maxSeats
      });
      return;
    }

    // --------------------------------------------------------------------------
    // 5. Customer Direct Claim / Recovery by Verified Order ID + Email
    // --------------------------------------------------------------------------
    if (pathname === '/api/v1/license/customer/claim-device' && req.method === 'POST') {
      if (!checkRateLimit(ip, 15, 10 * 60 * 1000)) {
        sendJson(res, 429, { success: false, error: 'Too many requests. Please try again shortly.' });
        return;
      }

      const { json: body } = await parseJsonBody(req);
      const { orderId, customerEmail, deviceId, deviceName, os, productId } = body;

      if (!orderId || !customerEmail || !deviceId) {
        sendJson(res, 400, { success: false, error: 'orderId, customerEmail and deviceId are required.' });
        return;
      }

      const cleanDevId = (deviceId || '').trim().toLowerCase().replace(/[^0-9a-f]/g, '');
      if (cleanDevId.length !== 64) {
        sendJson(res, 400, { success: false, error: 'Invalid 64-character device ID.' });
        return;
      }

      const cleanEmail = customerEmail.trim().toLowerCase();
      const cleanOrderId = orderId.trim();

      // Find purchase
      let purchase = null;
      if (productId) {
        const prod = resolveProduct(productId);
        if (prod) {
          purchase = db.prepare(`
            SELECT p.*, prod.name as prod_name, prod.slug as prod_slug, prod.brand as prod_brand, prod.plugin_identifier as prod_plugin_id
            FROM purchases p
            JOIN products prod ON p.product_id = prod.id
            WHERE p.order_id = ? AND LOWER(p.customer_email) = ? AND p.product_id = ? AND p.status = 'active'
          `).get(cleanOrderId, cleanEmail, prod.id);
        }
      }

      if (!purchase) {
        purchase = db.prepare(`
          SELECT p.*, prod.name as prod_name, prod.slug as prod_slug, prod.brand as prod_brand, prod.plugin_identifier as prod_plugin_id
          FROM purchases p
          JOIN products prod ON p.product_id = prod.id
          WHERE p.order_id = ? AND LOWER(p.customer_email) = ? AND p.status = 'active'
          LIMIT 1
        `).get(cleanOrderId, cleanEmail);
      }

      if (!purchase) {
        sendJson(res, 404, { success: false, error: 'No matching active order found for these details.' });
        return;
      }

      const product = {
        id: purchase.product_id,
        name: purchase.prod_name,
        slug: purchase.prod_slug,
        brand: purchase.prod_brand,
        plugin_identifier: purchase.prod_plugin_id
      };

      // Check existing seat
      const existing = db.prepare('SELECT * FROM device_activations WHERE serial_hash = ? AND device_id_hash = ?').get(purchase.serial_hash, cleanDevId);
      if (existing && existing.is_active) {
        sendJson(res, 200, {
          success: true,
          token: existing.token,
          productName: product.name,
          fileName: `${product.slug}.ptlic`
        });
        return;
      }

      const activeSeats = db.prepare('SELECT COUNT(*) as count FROM device_activations WHERE serial_hash = ? AND is_active = 1').get(purchase.serial_hash).count;
      if (activeSeats >= purchase.max_activation_seats) {
        sendJson(res, 403, {
          success: false,
          error: `Seat limit reached (${activeSeats}/${purchase.max_activation_seats}). Please deactivate an existing machine first.`
        });
        return;
      }

      const activationId = crypto.randomUUID();
      const entitlement = issueEntitlementToken(product, cleanDevId, purchase.customer_name, purchase.customer_email, activationId);

      db.prepare(`
        INSERT INTO device_activations (
          id, purchase_id, serial_hash, product_id, customer_email,
          device_id_hash, device_name, os, token, is_active,
          activated_at, last_seen_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 1, datetime('now'), datetime('now'))
      `).run(
        activationId, purchase.id, purchase.serial_hash, product.id, purchase.customer_email,
        cleanDevId, (deviceName || 'Studio Machine').slice(0, 64), os || 'Windows', entitlement.token
      );

      sendJson(res, 200, {
        success: true,
        token: entitlement.token,
        productName: product.name,
        fileName: `${product.slug}.ptlic`,
        seatsUsed: activeSeats + 1,
        maxSeats: purchase.max_activation_seats
      });
      return;
    }

    // --------------------------------------------------------------------------
    // 6. Offline License File Generation
    // --------------------------------------------------------------------------
    if (pathname === '/api/v1/license/offline/generate' && req.method === 'POST') {
      const { json: body } = await parseJsonBody(req);
      const { serialKey, deviceId, deviceName } = body;

      if (!serialKey || !deviceId) {
        sendJson(res, 400, { success: false, error: 'serialKey and deviceId are required.' });
        return;
      }

      const cleanDevId = deviceId.trim().toLowerCase().replace(/[^0-9a-f]/g, '');
      if (cleanDevId.length !== 64) {
        sendJson(res, 400, { success: false, error: 'Invalid 64-character Device ID.' });
        return;
      }

      const keyHash = hashSerial(serialKey);
      const purchase = db.prepare(`
        SELECT p.*, prod.name as prod_name, prod.slug as prod_slug, prod.brand as prod_brand, prod.plugin_identifier as prod_plugin_id
        FROM purchases p
        JOIN products prod ON p.product_id = prod.id
        WHERE p.serial_hash = ? AND p.status = 'active'
      `).get(keyHash);

      if (!purchase) {
        sendJson(res, 400, { success: false, error: 'Invalid or inactive serial key.' });
        return;
      }

      const product = {
        id: purchase.product_id,
        name: purchase.prod_name,
        slug: purchase.prod_slug,
        brand: purchase.prod_brand,
        plugin_identifier: purchase.prod_plugin_id
      };

      const existing = db.prepare('SELECT * FROM device_activations WHERE serial_hash = ? AND device_id_hash = ?').get(keyHash, cleanDevId);
      if (existing && existing.is_active) {
        sendJson(res, 200, {
          success: true,
          fileName: `${product.slug}.ptlic`,
          licenseToken: existing.token
        });
        return;
      }

      const seatCount = db.prepare('SELECT COUNT(*) as count FROM device_activations WHERE serial_hash = ? AND is_active = 1').get(keyHash).count;
      if (seatCount >= purchase.max_activation_seats) {
        sendJson(res, 403, {
          success: false,
          error: `Seat limit reached (${seatCount}/${purchase.max_activation_seats}). Deactivate an older device first.`
        });
        return;
      }

      const activationId = crypto.randomUUID();
      const entitlement = issueEntitlementToken(product, cleanDevId, purchase.customer_name, purchase.customer_email, activationId);

      db.prepare(`
        INSERT INTO device_activations (
          id, purchase_id, serial_hash, product_id, customer_email,
          device_id_hash, device_name, os, token, is_active,
          activated_at, last_seen_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, 'Offline Machine', ?, 1, datetime('now'), datetime('now'))
      `).run(activationId, purchase.id, keyHash, product.id, purchase.customer_email, cleanDevId, deviceName || 'Studio Workstation', entitlement.token);

      audit('offline_ptlic_downloaded', purchase.id, ip, { deviceIdPrefix: cleanDevId.slice(0, 8) });

      sendJson(res, 200, {
        success: true,
        fileName: `${product.slug}.ptlic`,
        licenseToken: entitlement.token
      });
      return;
    }

    // --------------------------------------------------------------------------
    // 7. Online Account Pairing - Start
    // --------------------------------------------------------------------------
    if (pathname === '/api/v1/license/pair/start' && req.method === 'POST') {
      if (!checkRateLimit(ip, 25, 10 * 60 * 1000)) {
        sendJson(res, 429, { success: false, error: 'Rate limit exceeded. Please wait.' });
        return;
      }

      const { json: body } = await parseJsonBody(req);
      const { productId, deviceId, deviceName, os } = body;

      const product = resolveProduct(productId) || resolveProduct(DEFAULT_PRODUCT_ID);
      const cleanDevId = (deviceId || '').trim().toLowerCase().replace(/[^0-9a-f]/g, '');
      if (cleanDevId.length !== 64) {
        sendJson(res, 400, { success: false, error: 'Invalid device ID format.' });
        return;
      }

      // Generate pairing code (e.g. PV-7B9A-2K4F)
      const part1 = crypto.randomBytes(2).toString('hex').toUpperCase();
      const part2 = crypto.randomBytes(2).toString('hex').toUpperCase();
      const code = `${product.serial_prefix.slice(0, 2)}-${part1}-${part2}`;
      const codeHash = crypto.createHash('sha256').update(code).digest('hex');
      const expiresAt = new Date(Date.now() + 5 * 60 * 1000).toISOString(); // 5 min TTL
      const challengeId = crypto.randomUUID();

      db.prepare(`
        INSERT INTO activation_challenges (
          id, challenge_code_hash, code, product_id,
          device_id_hash, device_name, os, status, expires_at, created_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, 'pending', ?, datetime('now'))
      `).run(challengeId, codeHash, code, product.id, cleanDevId, (deviceName || 'Studio DAW').slice(0, 64), os || 'Windows', expiresAt);

      audit('pair_challenge_started', challengeId, ip, { code, deviceIdPrefix: cleanDevId.slice(0, 8) });

      const claimBaseUrl = process.env.CLAIM_BASE_URL || 'https://licensing.producertoy.com/pair';
      sendJson(res, 200, {
        success: true,
        code,
        claimUrl: `${claimBaseUrl}?code=${code}`,
        expiresAt
      });
      return;
    }

    // --------------------------------------------------------------------------
    // 8. Online Account Pairing - Poll
    // --------------------------------------------------------------------------
    if (pathname === '/api/v1/license/pair/poll' && req.method === 'GET') {
      const code = url.searchParams.get('code');
      const deviceId = (url.searchParams.get('deviceId') || '').trim().toLowerCase().replace(/[^0-9a-f]/g, '');

      if (!code) {
        sendJson(res, 400, { success: false, error: 'code parameter required.' });
        return;
      }

      const codeHash = crypto.createHash('sha256').update(code.trim().toUpperCase()).digest('hex');
      const challenge = db.prepare('SELECT * FROM activation_challenges WHERE challenge_code_hash = ?').get(codeHash);

      if (!challenge) {
        sendJson(res, 404, { success: false, error: 'Pairing code not found or expired.' });
        return;
      }

      if (challenge.device_id_hash !== deviceId) {
        sendJson(res, 403, { success: false, error: 'Device mismatch for pairing session.' });
        return;
      }

      if (challenge.status === 'claimed') {
        const token = challenge.entitlement_token;
        db.prepare("UPDATE activation_challenges SET status = 'burned' WHERE id = ?").run(challenge.id);

        sendJson(res, 200, {
          success: true,
          status: 'claimed',
          token
        });
        return;
      }

      sendJson(res, 200, {
        success: true,
        status: challenge.status
      });
      return;
    }

    // --------------------------------------------------------------------------
    // 9. Online Account Pairing - Claim (Browser User / Verified Customer)
    // --------------------------------------------------------------------------
    if (pathname === '/api/v1/license/pair/claim' && req.method === 'POST') {
      const { json: body } = await parseJsonBody(req);
      const { code, serialKey, orderId, customerEmail, customerName } = body;

      if (!code) {
        sendJson(res, 400, { success: false, error: 'Pairing code is required.' });
        return;
      }

      const codeHash = crypto.createHash('sha256').update(code.trim().toUpperCase()).digest('hex');
      const challenge = db.prepare("SELECT * FROM activation_challenges WHERE challenge_code_hash = ? AND status = 'pending'").get(codeHash);

      if (!challenge) {
        sendJson(res, 404, { success: false, error: 'Pairing session is invalid or has expired.' });
        return;
      }

      if (new Date(challenge.expires_at).getTime() < Date.now()) {
        db.prepare("UPDATE activation_challenges SET status = 'expired' WHERE id = ?").run(challenge.id);
        sendJson(res, 410, { success: false, error: 'Pairing code has expired. Click Activate in plugin again.' });
        return;
      }

      // Security: Require either a valid serialKey OR a verified orderId + customerEmail proof
      // Rejecting plain email alone prevents unauthorized seat claiming
      let purchase = null;
      if (serialKey) {
        const keyHash = hashSerial(serialKey);
        purchase = db.prepare(`
          SELECT p.*, prod.name as prod_name, prod.slug as prod_slug, prod.brand as prod_brand, prod.plugin_identifier as prod_plugin_id
          FROM purchases p
          JOIN products prod ON p.product_id = prod.id
          WHERE p.serial_hash = ? AND p.product_id = ? AND p.status = 'active'
        `).get(keyHash, challenge.product_id);
      } else if (orderId && customerEmail) {
        purchase = db.prepare(`
          SELECT p.*, prod.name as prod_name, prod.slug as prod_slug, prod.brand as prod_brand, prod.plugin_identifier as prod_plugin_id
          FROM purchases p
          JOIN products prod ON p.product_id = prod.id
          WHERE p.order_id = ? AND LOWER(p.customer_email) = ? AND p.product_id = ? AND p.status = 'active'
        `).get(orderId.trim(), customerEmail.trim().toLowerCase(), challenge.product_id);
      }

      if (!purchase) {
        sendJson(res, 403, {
          success: false,
          error: 'No active license found for this product. Please enter your Serial Key or verified Order ID.'
        });
        return;
      }

      const activeSeats = db.prepare('SELECT COUNT(*) as count FROM device_activations WHERE serial_hash = ? AND is_active = 1').get(purchase.serial_hash).count;
      if (activeSeats >= purchase.max_activation_seats) {
        sendJson(res, 403, {
          success: false,
          error: `Seat limit reached (${activeSeats}/${purchase.max_activation_seats}). Deactivate an existing machine first.`
        });
        return;
      }

      const product = {
        id: purchase.product_id,
        name: purchase.prod_name,
        slug: purchase.prod_slug,
        brand: purchase.prod_brand,
        plugin_identifier: purchase.prod_plugin_id
      };

      const activationId = crypto.randomUUID();
      const entitlement = issueEntitlementToken(
        product,
        challenge.device_id_hash,
        customerName || purchase.customer_name,
        customerEmail || purchase.customer_email,
        activationId
      );

      db.exec('BEGIN IMMEDIATE');
      try {
        db.prepare(`
          INSERT INTO device_activations (
            id, purchase_id, serial_hash, product_id, customer_email,
            device_id_hash, device_name, os, token, is_active,
            activated_at, last_seen_at
          ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 1, datetime('now'), datetime('now'))
        `).run(
          activationId, purchase.id, purchase.serial_hash, product.id,
          customerEmail || purchase.customer_email, challenge.device_id_hash,
          challenge.device_name, challenge.os || 'Windows', entitlement.token
        );

        db.prepare(`
          UPDATE activation_challenges
          SET status = 'claimed', customer_email = ?, entitlement_token = ?, claimed_at = datetime('now')
          WHERE id = ?
        `).run(customerEmail || purchase.customer_email, entitlement.token, challenge.id);

        db.exec('COMMIT');
      } catch (err) {
        db.exec('ROLLBACK');
        sendJson(res, 500, { success: false, error: err.message });
        return;
      }

      audit('pair_session_claimed', challenge.id, ip, {
        customerEmail: maskEmail(customerEmail || purchase.customer_email),
        deviceIdPrefix: challenge.device_id_hash.slice(0, 8)
      });

      sendJson(res, 200, {
        success: true,
        message: 'Device successfully paired and licensed.'
      });
      return;
    }

    // --------------------------------------------------------------------------
    // 10. Deactivation Endpoint (Plugin GUI "Deactivate This Machine")
    // --------------------------------------------------------------------------
    if (pathname === '/api/v1/license/deactivate' && req.method === 'POST') {
      const { json: body } = await parseJsonBody(req);
      const { productId, deviceId, activationId } = body;

      const cleanDevId = (deviceId || '').trim().toLowerCase().replace(/[^0-9a-f]/g, '');
      let deactivated = false;

      if (cleanDevId) {
        let stmt;
        if (productId) {
          const prod = resolveProduct(productId);
          stmt = db.prepare(`
            UPDATE device_activations
            SET is_active = 0, revoked_at = datetime('now')
            WHERE device_id_hash = ? AND product_id = ? AND is_active = 1
          `);
          const info = stmt.run(cleanDevId, prod ? prod.id : productId);
          deactivated = info.changes > 0;
        } else {
          stmt = db.prepare(`
            UPDATE device_activations
            SET is_active = 0, revoked_at = datetime('now')
            WHERE device_id_hash = ? AND is_active = 1
          `);
          const info = stmt.run(cleanDevId);
          deactivated = info.changes > 0;
        }
      }

      audit('device_deactivated', activationId || cleanDevId, ip, {
        deviceIdPrefix: cleanDevId.slice(0, 8),
        deactivated
      });

      sendJson(res, 200, {
        success: true,
        deactivated,
        message: deactivated ? 'Seat successfully released on server.' : 'No active seat found for this device.'
      });
      return;
    }

    // --------------------------------------------------------------------------
    // 11. Admin Endpoints: Manage Marketplace Products
    // --------------------------------------------------------------------------
    if (pathname === '/api/v1/admin/products' && req.method === 'GET') {
      const auth = req.headers['authorization'];
      if (!auth || auth !== `Bearer ${ADMIN_API_KEY}`) {
        sendJson(res, 401, { success: false, error: 'Unauthorized.' });
        return;
      }
      const products = listAllProducts();
      sendJson(res, 200, { success: true, count: products.length, products });
      return;
    }

    if (pathname === '/api/v1/admin/products' && req.method === 'POST') {
      const auth = req.headers['authorization'];
      if (!auth || auth !== `Bearer ${ADMIN_API_KEY}`) {
        sendJson(res, 401, { success: false, error: 'Unauthorized.' });
        return;
      }

      const { json: body } = await parseJsonBody(req);
      const product = getOrRegisterProduct({
        id: body.id,
        name: body.name,
        slug: body.slug,
        brand: body.brand,
        pluginIdentifier: body.pluginIdentifier || body.plugin_identifier,
        prefix: body.prefix || body.serial_prefix,
        seats: body.maxSeats || body.max_activation_seats,
        graceDays: body.graceDays || body.offline_grace_days
      });

      sendJson(res, 200, { success: true, product });
      return;
    }

    // --------------------------------------------------------------------------
    // 12. Admin Endpoint: Manual Batch Serial Issuance (Any Product)
    // --------------------------------------------------------------------------
    if (pathname === '/api/v1/admin/serials/generate' && req.method === 'POST') {
      const auth = req.headers['authorization'];
      if (!auth || auth !== `Bearer ${ADMIN_API_KEY}`) {
        audit('admin_unauthorized_attempt', null, ip);
        sendJson(res, 401, { success: false, error: 'Unauthorized.' });
        return;
      }

      const { json: body } = await parseJsonBody(req);
      const product = resolveProduct(body.productId || body.slug || body.pluginId) || resolveProduct(DEFAULT_PRODUCT_ID);
      const count = Math.min(parseInt(body.count || '1', 10), 100);
      const maxSeats = parseInt(body.maxSeats || String(product.max_activation_seats || DEFAULT_MAX_SEATS), 10);
      const customerName = body.customerName || 'Licensed Customer';
      const customerEmail = (body.customerEmail || 'customer@producertoy.com').trim().toLowerCase();
      const orderId = body.orderId || `admin_${Date.now()}`;

      const generatedSerials = [];
      db.exec('BEGIN IMMEDIATE');
      try {
        const insertStmt = db.prepare(`
          INSERT INTO purchases (
            id, serial_hash, order_id, event_id, payment_id,
            product_id, plugin_identifier, customer_email, customer_name,
            max_activation_seats, status, created_at, updated_at
          ) VALUES (?, ?, ?, ?, 'admin_manual', ?, ?, ?, ?, ?, 'active', datetime('now'), datetime('now'))
        `);

        for (let i = 0; i < count; i++) {
          const serial = generateCryptographicSerial(product.serial_prefix);
          const serialHash = hashSerial(serial);
          const purchaseId = crypto.randomUUID();
          const eventId = `admin_${purchaseId}`;

          insertStmt.run(
            purchaseId, serialHash, orderId, eventId,
            product.id, product.plugin_identifier, customerEmail, customerName, maxSeats
          );
          generatedSerials.push(serial);
        }

        db.exec('COMMIT');
      } catch (err) {
        db.exec('ROLLBACK');
        sendJson(res, 500, { success: false, error: err.message });
        return;
      }

      audit('admin_serials_generated', orderId, ip, {
        productId: product.id,
        count,
        customerEmail: maskEmail(customerEmail)
      });

      sendJson(res, 200, {
        success: true,
        product: { id: product.id, name: product.name, brand: product.brand },
        count: generatedSerials.length,
        serials: generatedSerials
      });
      return;
    }

    // 404 Not Found
    sendJson(res, 404, { success: false, error: 'Endpoint not found.' });

  } catch (err) {
    console.error('[UNCAUGHT SERVER ERROR]', err);
    sendJson(res, 500, { success: false, error: 'Internal server error.' });
  }
});

server.listen(PORT, HOST, () => {
  console.log(`================================================================`);
  console.log(`  ProducerToy Multi-Brand Standalone Licensing Server (v2.1)    `);
  console.log(`  Zero Vercel / Zero Supabase Dependencies                      `);
  console.log(`================================================================`);
  console.log(`  Listening on: http://${HOST}:${PORT}`);
  console.log(`  Database: ${DB_FILE} (SQLite WAL)`);
  console.log(`  Default Product: ${DEFAULT_PRODUCT_NAME} [${DEFAULT_PRODUCT_ID}]`);
  console.log(`  Ed25519 Signing: Active [${KEY_ID}]`);
  console.log(`  Razorpay Webhook: ${RAZORPAY_WEBHOOK_SECRET ? 'Configured' : 'Missing secret'}`);
  console.log(`================================================================`);
});
