import os
import json
import hmac
import hashlib
import base64

def b64url(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).rstrip(b'=').decode('utf-8')

def main():
    payload = {
        "v": 1,
        "pid": "92cf2454-2cfc-4db5-b210-2c732457605f",
        "plugin_id": "com.producertoy.proverb",
        "aid": "94919a00-07de-4659-a99b-25d1d44cbf40",
        "uid_hash": "74f3da94ca4e3e9a9f9b9db82ef474c2d5cd7bbdd843351240141577c9551ba3",
        "dev_hash": "fd f3 6c 53 81 8f 1f ef 98 ad ed 19 cc 79 09 07 53 15 4c 60 76 94 76 1d 75 41 2a 50 1d 04 82 97",
        "user_name": "Naiem Shaikh",
        "user_email_masked": "n***l@gmail.com",
        "tier": "perpetual",
        "iat": 1791996740,
        "grace_days": 14
    }

    # Format JSON exactly compact
    payload_json = json.dumps(payload, separators=(',', ':'))
    enc_payload = b64url(payload_json.encode('utf-8'))

    # Secret in compiled binary
    master_secret = "3qwpro3wqu3u3uodnnxcv82384903898"
    sig_raw = hmac.new(master_secret.encode('utf-8'), enc_payload.encode('utf-8'), hashlib.sha256).digest()
    enc_sig = b64url(sig_raw)

    token = f"{enc_payload}.{enc_sig}"

    appdata = os.environ.get('APPDATA', '')
    target_dir = os.path.join(appdata, "ProducerToy", "Licenses")
    os.makedirs(target_dir, exist_ok=True)
    target_file = os.path.join(target_dir, "proverb.ptlic")

    with open(target_file, "w", encoding="utf-8") as f:
        f.write(token)

    print("=== PROVERB LICENSE GENERATION & VERIFICATION ===")
    print(f"Target File: {target_file}")
    print(f"File Size:   {os.path.getsize(target_file)} bytes")
    print(f"Token:       {token}")
    print(f"Signature:   {enc_sig}")

if __name__ == '__main__':
    main()
