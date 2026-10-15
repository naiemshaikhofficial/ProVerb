# ProVerb Dedicated Standalone Licensing Server

This is the **zero-Vercel / zero-Supabase** self-hosted licensing backend for the ProVerb audio plugin. It runs as an independent Node.js microservice on any VPS, dedicated server, or Docker container.

---

## Architecture & Security Principles

1. **Zero Vercel / Zero Supabase Traffic**
   - The plugin communicates directly with this dedicated endpoint (e.g., `https://licensing.producertoy.com/api/v1/license`).
   - Neither Vercel serverless nor Supabase edge functions are involved in license activation, serial redemption, or deactivation.

2. **Untrusted Client Binary**
   - The plugin binary contains **only the Ed25519 public key** (`874392bb5c67a3e2d91af3bf4d4de8204b39e85684df76900f87a880c8fbd2e9`).
   - The **private signing key is strictly server-only** and never distributed to client plugins, installers, or repositories.

3. **Plaintext Serials Are Never Stored**
   - Serial keys (`PT-PROV-XXXX-XXXX-XXXX-XXXX`) are hashed using a keyed HMAC-SHA256 with a protected server salt secret.
   - Even in the event of a database leak, plaintext serial keys cannot be recovered.

4. **Multi-Seat & Idempotent Redemption**
   - Each serial key permits a configurable seat limit (default: 3 devices).
   - Re-activating on the same machine returns the existing signed entitlement without consuming additional seats.

5. **Rate Limiting & Anti-Brute Force**
   - Failed serial attempts are throttled per IP to prevent automated guessing.
   - Pairing codes expire after 5 minutes and are single-use with replay protection.

---

## Environment Variables

| Variable | Required | Description | Example |
| :--- | :--- | :--- | :--- |
| `PORT` | No (default: 4000) | HTTP port the server binds to | `4000` |
| `HOST` | No (default: 0.0.0.0) | Interface to listen on | `127.0.0.1` |
| `DATA_DIR` | No | Directory for local persistence | `/var/lib/proverb-licensing` |
| `PROVERB_SIGNING_KEY` | Yes | Path to Ed25519 private key PEM file | `/etc/ssl/proverb/proverb-private.pem` |
| `LICENSING_SALT_SECRET` | Yes | Server secret for serial HMAC-SHA256 | `long-random-hex-string` |
| `LICENSING_ADMIN_KEY` | Yes | Bearer token for admin API | `secret-admin-token` |
| `CLAIM_BASE_URL` | No | Browser URL where users claim pairing | `https://licensing.producertoy.com/pair` |

---

## Deployment on Linux VPS (Ubuntu / Debian)

### 1. Install Node.js (>= 18)
```bash
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt-get install -y nodejs
```

### 2. Copy Licensing Server Files
```bash
sudo mkdir -p /opt/proverb-licensing
sudo cp -r server.mjs package.json /opt/proverb-licensing/
```

### 3. Store Private Key Securely
```bash
sudo mkdir -p /etc/proverb-licensing
# Copy your private key to /etc/proverb-licensing/proverb-ed25519-private.pem
sudo chmod 600 /etc/proverb-licensing/proverb-ed25519-private.pem
sudo chown -R www-data:www-data /etc/proverb-licensing
```

### 4. Setup Systemd Service
Create `/etc/systemd/system/proverb-licensing.service`:
```ini
[Unit]
Description=ProVerb Standalone Licensing Service
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
Environment=PROVERB_SIGNING_KEY=/etc/proverb-licensing/proverb-ed25519-private.pem
Environment=LICENSING_SALT_SECRET=your_high_entropy_salt_secret
Environment=LICENSING_ADMIN_KEY=your_secure_admin_bearer_token

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

### 5. Nginx Reverse Proxy & SSL (Let's Encrypt)
```nginx
server {
    server_name licensing.producertoy.com;

    location / {
        proxy_pass http://127.0.0.1:4000;
        proxy_http_version 1.1;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```
Obtain SSL certificate with Certbot:
```bash
sudo certbot --nginx -d licensing.producertoy.com
```

---

## API Contract

### 1. Serial Activation
- **POST** `/api/v1/license/serial/activate`
- **Request:**
  ```json
  {
    "productId": "92cf2454-2cfc-4db5-b210-2c732457605f",
    "serialKey": "PT-PROV-ABCD-EF01-2345-6789",
    "deviceId": "64_char_lowercase_hex_device_id",
    "deviceName": "Studio Mac Studio"
  }
  ```
- **Response (200 OK):**
  ```json
  {
    "success": true,
    "token": "payload_base64url.signature_base64url",
    "seatsUsed": 1,
    "maxSeats": 3
  }
  ```

### 2. Online Pairing Flow
- **Step 1: Start Pairing**
  - **POST** `/api/v1/license/pair/start`
  - Input: `{ "productId": "...", "deviceId": "...", "deviceName": "..." }`
  - Returns: `{ "success": true, "code": "PV-A1B2-C3D4", "claimUrl": "...", "expiresAt": "..." }`
- **Step 2: Poll Status**
  - **GET** `/api/v1/license/pair/poll?code=PV-A1B2-C3D4&deviceId=...`
  - Returns: `{ "success": true, "status": "pending" | "claimed" | "expired", "token": "..." }`
- **Step 3: User Claims Code in Browser**
  - **POST** `/api/v1/license/pair/claim`
  - Input: `{ "code": "PV-A1B2-C3D4", "userName": "John Doe", "userEmail": "john@example.com" }`

### 3. Deactivation
- **POST** `/api/v1/license/deactivate`
- **Request:**
  ```json
  {
    "productId": "92cf2454-2cfc-4db5-b210-2c732457605f",
    "activationId": "uuid",
    "deviceId": "64_char_lowercase_hex_device_id"
  }
  ```
- Releases device seat.
