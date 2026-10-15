#!/usr/bin/env node
/**
 * ProVerb Dedicated Standalone Licensing Server
 * Zero Vercel / Zero Supabase Dependencies
 * 
 * Runs independently on any Linux VPS / self-hosted server with Node.js >= 18.
 */

import http from 'node:http';
import https from 'node:https';
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Product Constants
const PRODUCT_ID = '92cf2454-2cfc-4db5-b210-2c732457605f';
const PLUGIN_ID = 'com.producertoy.proverb';
const KEY_ID = 'proverb-ed25519-2026-10-v2';

// Environment & Configuration
const PORT = parseInt(process.env.PORT || '4000', 10);
const HOST = process.env.HOST || '0.0.0.0';
const DATA_DIR = process.env.DATA_DIR || path.join(__dirname, 'data');
const DB_FILE = path.join(DATA_DIR, 'licensing_store.json');
const SALT_SECRET = process.env.LICENSING_SALT_SECRET || 'proverb-licensing-salt-secret-change-in-production';
const ADMIN_API_KEY = process.env.LICENSING_ADMIN_KEY || '';

// Load Ed25519 Private Key
function loadSigningKey() {
  let pem = process.env.PROVERB_ED25519_PRIVATE_KEY_PEM;
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
    console.warn('[WARN] No Ed25519 private key found! Set PROVERB_SIGNING_KEY or PROVERB_ED25519_PRIVATE_KEY_PEM.');
    return null;
  }
  try {
    const key = crypto.createPrivateKey(pem);
    if (key.asymmetricKeyType !== 'ed25519') {
      throw new Error('Key is not an Ed25519 private key');
    }
    return key;
  } catch (err) {
    console.error('[ERROR] Failed to load Ed25519 private key:', err.message);
    return null;
  }
}

const signingKey = loadSigningKey();

// In-Memory Database with atomic persistence to disk
let db = {
  serials: {},      // keyedHash -> { maxSeats, seatsUsed, devices: { deviceId: { activationId, activatedAt, deviceName, token } }, userName, userEmail }
  pairings: {},     // code -> { deviceId, deviceName, createdAt, expiresAt, status: 'pending'|'claimed'|'expired', token, aid }
  rateLimits: {},   // ip -> { count, windowStart }
  auditLog: []      // array of audit events (safe, no secrets)
};

function ensureDataDir() {
  if (!fs.existsSync(DATA_DIR)) {
    fs.mkdirSync(DATA_DIR, { recursive: true });
  }
}

function loadDatabase() {
  ensureDataDir();
  if (fs.existsSync(DB_FILE)) {
    try {
      const raw = fs.readFileSync(DB_FILE, 'utf8');
      const loaded = JSON.parse(raw);
      db.serials = loaded.serials || {};
      db.pairings = loaded.pairings || {};
      db.auditLog = (loaded.auditLog || []).slice(-1000); // keep last 1000 events
    } catch (err) {
      console.error('[ERROR] Failed to load DB file, starting fresh:', err.message);
    }
  }
}

function saveDatabase() {
  ensureDataDir();
  const tmpFile = DB_FILE + '.tmp';
  try {
    const data = JSON.stringify({
      serials: db.serials,
      pairings: db.pairings,
      auditLog: db.auditLog.slice(-1000)
    }, null, 2);
    fs.writeFileSync(tmpFile, data, 'utf8');
    fs.renameSync(tmpFile, DB_FILE);
  } catch (err) {
    console.error('[ERROR] Failed to save DB file:', err.message);
  }
}

loadDatabase();

// Keyed Hash for Serial Keys (NEVER store plaintext serials)
function hashSerial(serialKey) {
  const normalized = serialKey.trim().toUpperCase().replace(/[^A-Z0-9]/g, '');
  return crypto.createHmac('sha256', SALT_SECRET).update(normalized).digest('hex');
}

// Mask Email for privacy
function maskEmail(email) {
  if (!email || !email.includes('@')) return 'customer@producertoy.com';
  const parts = email.split('@');
  const local = parts[0];
  const domain = parts[1];
  const visible = local.length <= 2 ? local[0] : local[0] + '***' + local[local.length - 1];
  return visible + '@' + domain;
}

// Issue signed Ed25519 token
function issueEntitlementToken(deviceId, userName, userEmail, activationId = crypto.randomUUID()) {
  if (!signingKey) {
    throw new Error('Server signing key is not loaded');
  }
  const now = Math.floor(Date.now() / 1000);
  const payload = {
    v: 2,
    iss: 'producertoy.com',
    aud: PLUGIN_ID,
    pid: PRODUCT_ID,
    plugin_id: PLUGIN_ID,
    kid: KEY_ID,
    status: 'active',
    aid: activationId,
    dev_hash: deviceId.trim().toLowerCase().replace(/[^0-9a-f]/g, ''),
    user_name: userName || 'Licensed ProVerb User',
    user_email_masked: maskEmail(userEmail),
    tier: 'perpetual',
    iat: now,
    nbf: now,
    exp: 0
  };

  const encodedPayload = Buffer.from(JSON.stringify(payload), 'utf8').toString('base64url');
  const signature = crypto.sign(null, Buffer.from(encodedPayload, 'ascii'), signingKey).toString('base64url');
  return {
    token: encodedPayload + '.' + signature,
    activationId,
    payload
  };
}

// Security: Rate Limiting
function checkRateLimit(ip, maxAttempts = 10, windowMs = 15 * 60 * 1000) {
  const now = Date.now();
  const entry = db.rateLimits[ip] || { count: 0, windowStart: now };
  if (now - entry.windowStart > windowMs) {
    entry.count = 1;
    entry.windowStart = now;
  } else {
    entry.count += 1;
  }
  db.rateLimits[ip] = entry;
  return entry.count <= maxAttempts;
}

function audit(event, meta = {}) {
  const timestamp = new Date().toISOString();
  db.auditLog.push({ timestamp, event, ...meta });
  console.log(`[AUDIT] ${timestamp} - ${event} ${JSON.stringify(meta)}`);
  if (db.auditLog.length % 10 === 0) {
    saveDatabase();
  }
}

// Clean expired pairing codes every 60 seconds
setInterval(() => {
  const now = Date.now();
  let changed = false;
  for (const [code, p] of Object.entries(db.pairings)) {
    if (p.expiresAt < now && p.status === 'pending') {
      p.status = 'expired';
      changed = true;
    }
  }
  if (changed) saveDatabase();
}, 60000);

// Helper to parse JSON body
function parseJsonBody(req) {
  return new Promise((resolve, reject) => {
    let body = '';
    req.on('data', chunk => {
      body += chunk;
      if (body.length > 65536) { // 64KB max body
        req.destroy();
        reject(new Error('Payload too large'));
      }
    });
    req.on('end', () => {
      try {
        resolve(body ? JSON.parse(body) : {});
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

// HTTP Server
const server = http.createServer(async (req, res) => {
  const ip = req.headers['x-forwarded-for']?.split(',')[0].trim() || req.socket.remoteAddress || 'unknown';
  const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
  const pathname = url.pathname;

  // CORS Headers for API calls
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization, Accept');

  if (req.method === 'OPTIONS') {
    res.writeHead(204);
    res.end();
    return;
  }

  try {
    // 1. Health & Status
    if (pathname === '/health' || pathname === '/api/v1/health') {
      sendJson(res, 200, {
        status: 'ok',
        service: 'ProVerb Dedicated Licensing Server',
        signingKeyLoaded: !!signingKey,
        keyId: KEY_ID
      });
      return;
    }

    // 2. Serial Key Activation (Method 1)
    if (pathname === '/api/v1/license/serial/activate' && req.method === 'POST') {
      if (!checkRateLimit(ip, 10, 15 * 60 * 1000)) {
        audit('serial_rate_limited', { ip });
        sendJson(res, 429, { success: false, error: 'Too many requests. Please try again in 15 minutes.' });
        return;
      }

      const body = await parseJsonBody(req);
      const { productId, serialKey, deviceId, deviceName } = body;

      if (!productId || productId !== PRODUCT_ID) {
        sendJson(res, 400, { success: false, error: 'Invalid product identifier.' });
        return;
      }
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
      const serialEntry = db.serials[keyHash];

      if (!serialEntry) {
        audit('serial_invalid_attempt', { ip, deviceIdPrefix: cleanDevId.slice(0, 8) });
        sendJson(res, 400, { success: false, error: 'The serial key entered is invalid.' });
        return;
      }

      serialEntry.devices = serialEntry.devices || {};
      const maxSeats = serialEntry.maxSeats || 3;

      // Idempotency: Device already activated with this serial? Return existing token
      if (serialEntry.devices[cleanDevId]) {
        const existing = serialEntry.devices[cleanDevId];
        audit('serial_idempotent_reissue', { deviceIdPrefix: cleanDevId.slice(0, 8) });
        sendJson(res, 200, {
          success: true,
          token: existing.token,
          seatsUsed: Object.keys(serialEntry.devices).length,
          maxSeats
        });
        return;
      }

      // Check seat capacity
      const currentSeats = Object.keys(serialEntry.devices).length;
      if (currentSeats >= maxSeats) {
        audit('serial_seats_exceeded', { deviceIdPrefix: cleanDevId.slice(0, 8), currentSeats, maxSeats });
        sendJson(res, 403, {
          success: false,
          error: `Seat limit reached (${currentSeats}/${maxSeats}). Please deactivate an existing machine first.`
        });
        return;
      }

      // Issue entitlement
      const entitlement = issueEntitlementToken(cleanDevId, serialEntry.userName, serialEntry.userEmail);
      serialEntry.devices[cleanDevId] = {
        activationId: entitlement.activationId,
        activatedAt: new Date().toISOString(),
        deviceName: (deviceName || 'Studio DAW').slice(0, 64),
        token: entitlement.token
      };
      saveDatabase();

      audit('serial_activated_success', {
        deviceIdPrefix: cleanDevId.slice(0, 8),
        seatsUsed: Object.keys(serialEntry.devices).length,
        maxSeats
      });

      sendJson(res, 200, {
        success: true,
        token: entitlement.token,
        seatsUsed: Object.keys(serialEntry.devices).length,
        maxSeats
      });
      return;
    }

    // 3. Online Account Pairing - Start (Method 2)
    if (pathname === '/api/v1/license/pair/start' && req.method === 'POST') {
      if (!checkRateLimit(ip, 20, 10 * 60 * 1000)) {
        sendJson(res, 429, { success: false, error: 'Too many requests. Please wait a few minutes.' });
        return;
      }

      const body = await parseJsonBody(req);
      const { productId, deviceId, deviceName } = body;

      if (!productId || productId !== PRODUCT_ID) {
        sendJson(res, 400, { success: false, error: 'Invalid product identifier.' });
        return;
      }
      const cleanDevId = (deviceId || '').trim().toLowerCase().replace(/[^0-9a-f]/g, '');
      if (cleanDevId.length !== 64) {
        sendJson(res, 400, { success: false, error: 'Invalid hardware device ID format.' });
        return;
      }

      // Generate 8-character high-entropy uppercase code (e.g., PV8K-39MZ)
      const part1 = crypto.randomBytes(2).toString('hex').toUpperCase();
      const part2 = crypto.randomBytes(2).toString('hex').toUpperCase();
      const code = `PV-${part1}-${part2}`;

      const now = Date.now();
      const expiresAt = now + (5 * 60 * 1000); // 5 minutes TTL

      db.pairings[code] = {
        code,
        deviceId: cleanDevId,
        deviceName: (deviceName || 'Studio DAW').slice(0, 64),
        createdAt: now,
        expiresAt,
        status: 'pending',
        token: null,
        aid: null
      };
      saveDatabase();

      audit('pair_session_started', { code, deviceIdPrefix: cleanDevId.slice(0, 8) });

      const claimBaseUrl = process.env.CLAIM_BASE_URL || 'https://licensing.producertoy.com/pair';
      sendJson(res, 200, {
        success: true,
        code,
        claimUrl: `${claimBaseUrl}?code=${code}`,
        expiresAt: new Date(expiresAt).toISOString()
      });
      return;
    }

    // 4. Online Account Pairing - Poll
    if (pathname === '/api/v1/license/pair/poll' && req.method === 'GET') {
      const code = url.searchParams.get('code');
      const deviceId = (url.searchParams.get('deviceId') || '').trim().toLowerCase().replace(/[^0-9a-f]/g, '');

      if (!code || !db.pairings[code]) {
        sendJson(res, 404, { success: false, error: 'Pairing code not found or expired.' });
        return;
      }

      const session = db.pairings[code];
      if (session.deviceId !== deviceId) {
        sendJson(res, 403, { success: false, error: 'Device mismatch for this pairing session.' });
        return;
      }

      if (Date.now() > session.expiresAt && session.status === 'pending') {
        session.status = 'expired';
        saveDatabase();
      }

      if (session.status === 'claimed') {
        const token = session.token;
        // Replay protection: Burn the code once handed over to the plugin
        delete db.pairings[code];
        saveDatabase();

        sendJson(res, 200, {
          success: true,
          status: 'claimed',
          token
        });
        return;
      }

      sendJson(res, 200, {
        success: true,
        status: session.status
      });
      return;
    }

    // 5. Online Account Pairing - Claim (Browser User Endpoint)
    if (pathname === '/api/v1/license/pair/claim' && req.method === 'POST') {
      const body = await parseJsonBody(req);
      const { code, userName, userEmail } = body;

      if (!code || !db.pairings[code]) {
        sendJson(res, 404, { success: false, error: 'Pairing code not found or has expired.' });
        return;
      }

      const session = db.pairings[code];
      if (session.status !== 'pending' || Date.now() > session.expiresAt) {
        sendJson(res, 400, { success: false, error: 'Pairing session is no longer active.' });
        return;
      }

      const entitlement = issueEntitlementToken(
        session.deviceId,
        userName || 'ProducerToy Account Holder',
        userEmail || 'user@producertoy.com'
      );

      session.status = 'claimed';
      session.token = entitlement.token;
      session.aid = entitlement.activationId;
      saveDatabase();

      audit('pair_session_claimed', { code, deviceIdPrefix: session.deviceId.slice(0, 8) });

      sendJson(res, 200, {
        success: true,
        message: 'Device successfully paired and licensed.'
      });
      return;
    }

    // 6. Deactivation Endpoint
    if (pathname === '/api/v1/license/deactivate' && req.method === 'POST') {
      const body = await parseJsonBody(req);
      const { productId, activationId, deviceId } = body;

      if (!productId || productId !== PRODUCT_ID) {
        sendJson(res, 400, { success: false, error: 'Invalid product identifier.' });
        return;
      }

      const cleanDevId = (deviceId || '').trim().toLowerCase().replace(/[^0-9a-f]/g, '');
      let deactivated = false;

      // Find and remove matching seat across serials
      for (const [keyHash, entry] of Object.entries(db.serials)) {
        if (entry.devices && entry.devices[cleanDevId]) {
          delete entry.devices[cleanDevId];
          deactivated = true;
          audit('serial_device_deactivated', {
            activationId,
            deviceIdPrefix: cleanDevId.slice(0, 8)
          });
        }
      }

      saveDatabase();
      sendJson(res, 200, {
        success: true,
        deactivated,
        message: 'Seat released on server.'
      });
      return;
    }

    // 7. Admin Endpoint: Generate Serials
    if (pathname === '/api/v1/admin/serials/generate' && req.method === 'POST') {
      const auth = req.headers['authorization'];
      if (!ADMIN_API_KEY || auth !== `Bearer ${ADMIN_API_KEY}`) {
        sendJson(res, 401, { success: false, error: 'Unauthorized.' });
        return;
      }

      const body = await parseJsonBody(req);
      const count = Math.min(parseInt(body.count || '1', 10), 100);
      const maxSeats = parseInt(body.maxSeats || '3', 10);
      const userName = body.userName || 'Licensed Customer';
      const userEmail = body.userEmail || 'customer@producertoy.com';

      const generated = [];
      for (let i = 0; i < count; ++i) {
        // High entropy: 16 random base32/hex characters split into groups of 4: PT-PROV-XXXX-XXXX-XXXX-XXXX
        const b = crypto.randomBytes(10).toString('hex').toUpperCase();
        const serial = `PT-PROV-${b.slice(0, 4)}-${b.slice(4, 8)}-${b.slice(8, 12)}-${b.slice(12, 16)}`;
        const keyHash = hashSerial(serial);

        db.serials[keyHash] = {
          maxSeats,
          userName,
          userEmail,
          createdAt: new Date().toISOString(),
          devices: {}
        };
        generated.push(serial);
      }

      saveDatabase();
      audit('admin_serials_generated', { count, maxSeats });

      sendJson(res, 200, {
        success: true,
        count: generated.length,
        serials: generated // Outputted only once to admin; never stored in plaintext on server
      });
      return;
    }

    // 404 for unknown routes
    sendJson(res, 404, { success: false, error: 'Endpoint not found.' });

  } catch (err) {
    console.error('[SERVER ERROR]', err);
    sendJson(res, 500, { success: false, error: 'Internal server error.' });
  }
});

server.listen(PORT, HOST, () => {
  console.log(`=======================================================`);
  console.log(`  ProVerb Dedicated Standalone Licensing Server       `);
  console.log(`  Zero Vercel / Zero Supabase Dependencies             `);
  console.log(`=======================================================`);
  console.log(`  Listening on: http://${HOST}:${PORT}`);
  console.log(`  Data Directory: ${DATA_DIR}`);
  console.log(`  Signing Key Loaded: ${!!signingKey}`);
  console.log(`  Key ID: ${KEY_ID}`);
  console.log(`=======================================================`);
});
