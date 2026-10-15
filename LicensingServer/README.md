# ProducerToy ProVerb Standalone Licensing Server (v2.0)

This is the **zero-Vercel / zero-Supabase** self-hosted licensing backend for the ProVerb audio plugin. It runs as an independent Node.js microservice on any VPS, dedicated server, or Docker container.

---

## 1. Free Database Options & Zero-Cost Architecture

Aapko kisi paid database ya cloud lock-in ki zaroorat nahi hai:

| Database Option | Cost | Setup | Performance / Storage | Recommended When |
| :--- | :--- | :--- | :--- | :--- |
| **Local SQLite (WAL Mode)** *(Default)* | **100% Free Forever** ($0) | **0 Setup:** Built directly into Node.js runtime (`node:sqlite`). No external installs or npm packages. | **< 1ms response time**, ACID transactions, concurrent reads/writes via SQLite WAL. Data saved in `/data/licensing.db`. | **Recommended for VPS / self-hosting:** 100% self-contained, no external dependency, cannot be shut down by third parties. |
| **Turso (libSQL)** | **100% Free Tier** ($0/mo) | Free account at [turso.tech](https://turso.tech) (no credit card required). Run `turso db create proverb-licenses`. | 9 GB storage, 500 databases, 1 billion row reads/month, global edge replication. | When you want a managed remote database with offsite cloud backups and web dashboard. |
| **Cloudflare D1** | **100% Free Tier** ($0/mo) | Cloudflare account -> Workers & D1 -> Create database. | 5 million reads/day, 100,000 writes/day, 5 GB storage. | When hosting licensing endpoints on Cloudflare Workers/Edge. |

---

## 2. Security & Anti-Piracy Architecture

1. **Zero Vercel / Zero Supabase Traffic**
   - The plugin communicates directly with this dedicated endpoint (e.g., `https://licensing.producertoy.com/api/v1/license`).
   - Neither Vercel serverless nor Supabase edge functions are involved in license activation, serial redemption, or deactivation.
2. **Untrusted Client Binary**
   - The plugin binary contains **only the Ed25519 public key** (`874392bb5c67a3e2d91af3bf4d4de8204b39e85684df76900f87a880c8fbd2e9`).
   - The **private signing key is strictly server-only** and never distributed to client plugins, installers, or repositories.
3. **Plaintext Serials Are Never Stored**
   - Serial keys (`PT-PROV-XXXX-XXXX-XXXX-XXXX`, 128-bit cryptographic entropy) are hashed using a keyed HMAC-SHA256 with a protected server salt secret.
   - Even in the event of a database leak, plaintext serial keys cannot be recovered.
4. **Multi-Seat & Idempotent Redemption**
   - Each serial key permits a configurable seat limit (default: 3 devices).
   - Re-activating on the same machine returns the existing signed entitlement without consuming additional seats.
5. **Rate Limiting & Anti-Brute Force**
   - Failed serial attempts are throttled per IP to prevent automated guessing.
   - Reverse proxy headers (`X-Forwarded-For`) are only trusted when `TRUST_PROXY=true`.
   - Pairing codes expire after 5 minutes and are single-use with replay protection.

---

## 3. Database Schema (Matches Website Catalog)

The database schema directly maps to the ProducerToy marketplace schema:
- **`products`**: `id` (`92cf2454-2cfc-4db5-b210-2c732457605f`), `name` ('ProVerb'), `plugin_identifier` ('com.producertoy.proverb'), `max_activation_seats` (3), `offline_grace_days` (14).
- **`purchases`**: `id`, `serial_hash` (HMAC primary key), `order_id`, `event_id`, `payment_id`, `customer_email`, `customer_name`, `max_activation_seats`, `status` ('active', 'revoked', 'refunded').
- **`device_activations`**: `id`, `purchase_id`, `serial_hash`, `device_id_hash` (64-char hex), `device_name`, `os`, `token`, `is_active`, `activated_at`, `revoked_at`.
- **`activation_challenges`**: `id`, `challenge_code_hash`, `code`, `device_id_hash`, `status`, `expires_at`.
- **`idempotency_keys`**: `event_id`, `provider`, `processed_at`, `response_json`.
- **`audit_log`**: `id`, `event_type`, `reference_id`, `ip_address`, `meta_json`, `created_at`.

---

## 4. Payment Provider Webhook Setup (Razorpay)

Configure this webhook directly inside your Razorpay Dashboard:

1. **Webhook URL:** `https://licensing.producertoy.com/api/v1/license/webhook/razorpay`
2. **Secret:** Set the same secret in `RAZORPAY_WEBHOOK_SECRET` on your VPS.
3. **Active Events:**
   - `payment.captured` (Automatically issues a unique 128-bit serial key)
   - `order.paid`
   - `payment.refunded` (Automatically revokes the serial key and deactivates machine seats)

### Webhook Idempotency & Replay Defense
- Provider event ID (`event_id` or `payment.id`) is verified against the `idempotency_keys` table.
- If Razorpay retries the same webhook delivery, the server returns `200 OK` with `{ status: "already_processed" }` without issuing duplicate serials or emails.

---

## 5. Deployment Guide (Linux VPS with Systemd & Nginx)

### 1. Requirements
- Node.js >= 22 (for built-in `node:sqlite`)
- Nginx & Let's Encrypt Certbot
- Domain pointing to VPS: `licensing.producertoy.com`

### 2. Install Files
```bash
sudo mkdir -p /opt/proverb-licensing
sudo cp server.mjs package.json /opt/proverb-licensing/
```

### 3. Store Ed25519 Private Key
```bash
sudo mkdir -p /etc/proverb-licensing
sudo cp proverb-ed25519-private.pem /etc/proverb-licensing/
sudo chmod 600 /etc/proverb-licensing/proverb-ed25519-private.pem
sudo chown -R www-data:www-data /etc/proverb-licensing
```

### 4. Setup Systemd Service
Create `/etc/systemd/system/proverb-licensing.service`:
```ini
[Unit]
Description=ProducerToy ProVerb Licensing Service
After=network.target

[Service]
Type=simple
User=www-data
WorkingDirectory=/opt/proverb-licensing
ExecStart=/usr/bin/node server.mjs
Restart=always
RestartSec=5
Environment=PORT=4000
Environment=HOST=127.0.0.1
Environment=DATA_DIR=/var/lib/proverb-licensing
Environment=TRUST_PROXY=true
Environment=PROVERB_SIGNING_KEY=/etc/proverb-licensing/proverb-ed25519-private.pem
Environment=LICENSING_SALT_SECRET=your_32_character_high_entropy_secret_salt_here!
Environment=LICENSING_ADMIN_KEY=your_secure_admin_bearer_token
Environment=RAZORPAY_WEBHOOK_SECRET=your_razorpay_webhook_secret

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo mkdir -p /var/lib/proverb-licensing
sudo chown -R www-data:www-data /var/lib/proverb-licensing /opt/proverb-licensing
sudo systemctl daemon-reload
sudo systemctl enable --now proverb-licensing
```

### 5. Nginx Reverse Proxy with TLS
```nginx
server {
    server_name licensing.producertoy.com;

    location / {
        proxy_pass http://127.0.0.1:4000;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

```bash
sudo certbot --nginx -d licensing.producertoy.com
```

---

## 6. Database Backup & Disaster Recovery

SQLite WAL mode allows zero-downtime hot backups while the server is live:

```bash
# Automated daily backup cron job
sqlite3 /var/lib/proverb-licensing/licensing.db ".backup /var/backups/licensing_$(date +%F).db"
```

To restore:
```bash
sudo systemctl stop proverb-licensing
sudo cp /var/backups/licensing_2026-10-15.db /var/lib/proverb-licensing/licensing.db
sudo chown www-data:www-data /var/lib/proverb-licensing/licensing.db
sudo systemctl start proverb-licensing
```
