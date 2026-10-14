#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace ProducerToy::Crypto
{
    // ==============================================================================
    // Standard Compact SHA-512 Implementation (FIPS 180-4)
    // ==============================================================================
    class SHA512
    {
    public:
        SHA512() { reset(); }

        void reset()
        {
            state[0] = 0x6a09e667f3bcc908ULL;
            state[1] = 0xbb67ae8584caa73bULL;
            state[2] = 0x3c6ef372fe94f82bULL;
            state[3] = 0xa54ff53a5f1d36f1ULL;
            state[4] = 0x510e527fade682d1ULL;
            state[5] = 0x9b05688c2b3e6c1fULL;
            state[6] = 0x1f83d9abfb41bd6bULL;
            state[7] = 0x5be0cd19137e2179ULL;
            count[0] = count[1] = 0;
            bufferLen = 0;
        }

        void update(const uint8_t* data, size_t len)
        {
            for (size_t i = 0; i < len; ++i)
            {
                buffer[bufferLen++] = data[i];
                if (bufferLen == 128)
                {
                    processBlock(buffer);
                    bufferLen = 0;
                }
            }
            uint64_t bitLen = static_cast<uint64_t>(len) * 8ULL;
            count[0] += bitLen;
            if (count[0] < bitLen)
                count[1]++;
        }

        void finalize(uint8_t hash[64])
        {
            buffer[bufferLen++] = 0x80;
            if (bufferLen > 112)
            {
                while (bufferLen < 128)
                    buffer[bufferLen++] = 0;
                processBlock(buffer);
                bufferLen = 0;
            }
            while (bufferLen < 112)
                buffer[bufferLen++] = 0;

            for (int i = 0; i < 8; ++i)
                buffer[112 + i] = static_cast<uint8_t>(count[1] >> ((7 - i) * 8));
            for (int i = 0; i < 8; ++i)
                buffer[120 + i] = static_cast<uint8_t>(count[0] >> ((7 - i) * 8));

            processBlock(buffer);

            for (int i = 0; i < 8; ++i)
            {
                for (int j = 0; j < 8; ++j)
                {
                    hash[i * 8 + j] = static_cast<uint8_t>(state[i] >> ((7 - j) * 8));
                }
            }
        }

        static void hash(const uint8_t* data, size_t len, uint8_t output[64])
        {
            SHA512 ctx;
            ctx.update(data, len);
            ctx.finalize(output);
        }

    private:
        uint64_t state[8];
        uint64_t count[2];
        uint8_t buffer[128];
        size_t bufferLen;

        static inline uint64_t rotr(uint64_t x, int n) { return (x >> n) | (x << (64 - n)); }
        static inline uint64_t ch(uint64_t x, uint64_t y, uint64_t z) { return (x & y) ^ (~x & z); }
        static inline uint64_t maj(uint64_t x, uint64_t y, uint64_t z) { return (x & y) ^ (x & z) ^ (y & z); }
        static inline uint64_t sigma0(uint64_t x) { return rotr(x, 28) ^ rotr(x, 34) ^ rotr(x, 39); }
        static inline uint64_t sigma1(uint64_t x) { return rotr(x, 14) ^ rotr(x, 18) ^ rotr(x, 41); }
        static inline uint64_t gamma0(uint64_t x) { return rotr(x, 1) ^ rotr(x, 8) ^ (x >> 7); }
        static inline uint64_t gamma1(uint64_t x) { return rotr(x, 19) ^ rotr(x, 61) ^ (x >> 6); }

        void processBlock(const uint8_t block[128])
        {
            static const uint64_t K[80] = {
                0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
                0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
                0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
                0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
                0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
                0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
                0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
                0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
                0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
                0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47867d1ULL, 0x92722c851482353bULL,
                0x9c30d532ca4e7b81ULL, 0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL,
                0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL,
                0x106aa07032bbd1b8ULL, 0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeeb9ULL,
                0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL,
                0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL,
                0x8cc702081a6439ecULL, 0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL,
                0xc67178f2e372532bULL, 0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL,
                0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL,
                0x1b710b35131c471bULL, 0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
                0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL
            };

            uint64_t W[80];
            for (int i = 0; i < 16; ++i)
            {
                W[i] = (static_cast<uint64_t>(block[i * 8 + 0]) << 56) |
                       (static_cast<uint64_t>(block[i * 8 + 1]) << 48) |
                       (static_cast<uint64_t>(block[i * 8 + 2]) << 40) |
                       (static_cast<uint64_t>(block[i * 8 + 3]) << 32) |
                       (static_cast<uint64_t>(block[i * 8 + 4]) << 24) |
                       (static_cast<uint64_t>(block[i * 8 + 5]) << 16) |
                       (static_cast<uint64_t>(block[i * 8 + 6]) << 8) |
                       (static_cast<uint64_t>(block[i * 8 + 7]));
            }
            for (int i = 16; i < 80; ++i)
            {
                W[i] = gamma1(W[i - 2]) + W[i - 7] + gamma0(W[i - 15]) + W[i - 16];
            }

            uint64_t a = state[0], b = state[1], c = state[2], d = state[3];
            uint64_t e = state[4], f = state[5], g = state[6], h = state[7];

            for (int i = 0; i < 80; ++i)
            {
                uint64_t T1 = h + sigma1(e) + ch(e, f, g) + K[i] + W[i];
                uint64_t T2 = sigma0(a) + maj(a, b, c);
                h = g; g = f; f = e; e = d + T1;
                d = c; c = b; b = a; a = T1 + T2;
            }

            state[0] += a; state[1] += b; state[2] += c; state[3] += d;
            state[4] += e; state[5] += f; state[6] += g; state[7] += h;
        }
    };

    // ==============================================================================
    // Compact Ed25519 Curve Arithmetic & Verification (RFC 8032)
    // ==============================================================================
    namespace Ed25519
    {
        using gf = int64_t[16];

        static void set25519(gf r, const gf a) { for (int i = 0; i < 16; i++) r[i] = a[i]; }
        static void car25519(gf o)
        {
            for (int i = 0; i < 16; i++)
            {
                o[i] += (1LL << 16);
                int64_t c = o[i] >> 16;
                o[(i + 1) * (i < 15 ? 1 : 0)] += c - 1 + (i == 15 ? (c - 1) * 37 : 0);
                o[i] -= c << 16;
            }
        }

        static void sel25519(gf p, gf q, int b)
        {
            int64_t c = ~(b - 1);
            for (int i = 0; i < 16; i++)
            {
                int64_t t = c & (p[i] ^ q[i]);
                p[i] ^= t;
                q[i] ^= t;
            }
        }

        static void pack25519(uint8_t* o, const gf n)
        {
            gf m, t;
            set25519(t, n);
            car25519(t); car25519(t); car25519(t);
            for (int j = 0; j < 2; j++)
            {
                m[0] = t[0] - 0xffed;
                for (int i = 1; i < 15; i++)
                {
                    m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
                    m[i - 1] &= 0xffff;
                }
                m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
                m[14] &= 0xffff;
                int64_t b = (m[15] >> 16) & 1;
                m[15] &= 0xffff;
                sel25519(t, m, 1 - static_cast<int>(b));
            }
            for (int i = 0; i < 16; i++)
            {
                o[2 * i] = static_cast<uint8_t>(t[i] & 0xff);
                o[2 * i + 1] = static_cast<uint8_t>(t[i] >> 8);
            }
        }

        static void unpack25519(gf o, const uint8_t* n)
        {
            for (int i = 0; i < 16; i++)
                o[i] = n[2 * i] + (static_cast<int64_t>(n[2 * i + 1]) << 8);
            o[15] &= 0x7fff;
        }

        static void A(gf o, const gf a, const gf b) { for (int i = 0; i < 16; i++) o[i] = a[i] + b[i]; }
        static void Z(gf o, const gf a, const gf b) { for (int i = 0; i < 16; i++) o[i] = a[i] - b[i]; }

        static void M(gf o, const gf a, const gf b)
        {
            int64_t t[31] = { 0 };
            for (int i = 0; i < 16; i++)
                for (int j = 0; j < 16; j++)
                    t[i + j] += a[i] * b[j];
            for (int i = 0; i < 15; i++)
                t[i] += 38 * t[i + 16];
            for (int i = 0; i < 16; i++)
                o[i] = t[i];
            car25519(o);
            car25519(o);
        }

        static void S(gf o, const gf a) { M(o, a, a); }

        static void inv25519(gf o, const gf i)
        {
            gf c;
            set25519(c, i);
            for (int a = 253; a >= 0; a--)
            {
                S(c, c);
                if (a != 2 && a != 4)
                    M(c, c, i);
            }
            set25519(o, c);
        }

        static const gf d = {
            static_cast<int64_t>(0xeb35), static_cast<int64_t>(0x5341), static_cast<int64_t>(0x45d4), static_cast<int64_t>(0x69db),
            static_cast<int64_t>(0x4513), static_cast<int64_t>(0xa0ea), static_cast<int64_t>(0x00e1), static_cast<int64_t>(0x224e),
            static_cast<int64_t>(0x88e0), static_cast<int64_t>(0x5541), static_cast<int64_t>(0x2247), static_cast<int64_t>(0x1200),
            static_cast<int64_t>(0x7367), static_cast<int64_t>(0x344d), static_cast<int64_t>(0xef56), static_cast<int64_t>(0x5203)
        };

        static const gf I = {
            static_cast<int64_t>(0xa0b0), static_cast<int64_t>(0x4a0e), static_cast<int64_t>(0x1b27), static_cast<int64_t>(0xc4ee),
            static_cast<int64_t>(0xe478), static_cast<int64_t>(0xad2f), static_cast<int64_t>(0x1806), static_cast<int64_t>(0x2f43),
            static_cast<int64_t>(0xd7a7), static_cast<int64_t>(0x3dfb), static_cast<int64_t>(0x0099), static_cast<int64_t>(0x2b4d),
            static_cast<int64_t>(0xdf0b), static_cast<int64_t>(0x4fc1), static_cast<int64_t>(0x2480), static_cast<int64_t>(0x2b83)
        };

        static int unpackneg(gf r[4], const uint8_t p[32])
        {
            gf t, chk, num, den, den2, den4, den6;
            set25519(r[2], gf{ 1 });
            unpack25519(r[1], p);
            S(num, r[1]);
            M(den, num, d);
            Z(num, num, r[2]);
            A(den, r[2], den);

            S(den2, den);
            S(den4, den2);
            M(den6, den4, den2);
            M(t, den6, num);
            M(t, t, den);

            gf c;
            set25519(c, t);
            for (int a = 251; a >= 0; a--)
            {
                S(c, c);
                if (a != 1)
                    M(c, c, t);
            }
            M(r[0], c, num);
            M(r[0], r[0], den);

            S(chk, r[0]);
            M(chk, chk, den);
            if (memcmp(chk, num, sizeof(gf)) != 0)
                M(r[0], r[0], I);

            S(chk, r[0]);
            M(chk, chk, den);
            if (memcmp(chk, num, sizeof(gf)) != 0)
                return -1;

            if ((r[0][0] & 1) != (p[31] >> 7))
                Z(r[0], gf{ 0 }, r[0]);

            M(r[3], r[0], r[1]);
            return 0;
        }

        static void add(gf p[4], const gf q[4])
        {
            gf a, b, c, d_t, e, f, g, h, t;
            Z(a, p[1], p[0]);
            Z(t, q[1], q[0]);
            M(a, a, t);
            A(b, p[1], p[0]);
            A(t, q[1], q[0]);
            M(b, b, t);
            M(c, p[3], q[3]);
            M(c, c, d);
            M(c, c, gf{ 2 });
            M(d_t, p[2], q[2]);
            M(d_t, d_t, gf{ 2 });
            Z(e, b, a);
            Z(f, d_t, c);
            A(g, d_t, c);
            A(h, b, a);
            M(p[0], e, f);
            M(p[1], h, g);
            M(p[2], g, f);
            M(p[3], e, h);
        }

        static void scalarmult(gf p[4], const gf q[4], const uint8_t s[32])
        {
            set25519(p[0], gf{ 0 });
            set25519(p[1], gf{ 1 });
            set25519(p[2], gf{ 1 });
            set25519(p[3], gf{ 0 });

            for (int i = 255; i >= 0; --i)
            {
                int b = (s[i / 8] >> (i % 8)) & 1;
                add(p, p);
                if (b)
                    add(p, q);
            }
        }

        static const gf B_point[4] = {
            { static_cast<int64_t>(0xd51a), static_cast<int64_t>(0x8f25), static_cast<int64_t>(0x2d60), static_cast<int64_t>(0xc956),
              static_cast<int64_t>(0xa7b2), static_cast<int64_t>(0x9525), static_cast<int64_t>(0xc760), static_cast<int64_t>(0x692c),
              static_cast<int64_t>(0xdc5c), static_cast<int64_t>(0xfdd6), static_cast<int64_t>(0xe231), static_cast<int64_t>(0xc0a4),
              static_cast<int64_t>(0x53fe), static_cast<int64_t>(0xcd6e), static_cast<int64_t>(0x36d3), static_cast<int64_t>(0x2169) },
            { static_cast<int64_t>(0x6658), static_cast<int64_t>(0x6666), static_cast<int64_t>(0x6666), static_cast<int64_t>(0x6666),
              static_cast<int64_t>(0x6666), static_cast<int64_t>(0x6666), static_cast<int64_t>(0x6666), static_cast<int64_t>(0x6666),
              static_cast<int64_t>(0x6666), static_cast<int64_t>(0x6666), static_cast<int64_t>(0x6666), static_cast<int64_t>(0x6666),
              static_cast<int64_t>(0x6666), static_cast<int64_t>(0x6666), static_cast<int64_t>(0x6666), static_cast<int64_t>(0x6666) },
            { 1 },
            { static_cast<int64_t>(0xa5b7), static_cast<int64_t>(0x5555), static_cast<int64_t>(0x5555), static_cast<int64_t>(0x5555),
              static_cast<int64_t>(0x5555), static_cast<int64_t>(0x5555), static_cast<int64_t>(0x5555), static_cast<int64_t>(0x5555),
              static_cast<int64_t>(0x5555), static_cast<int64_t>(0x5555), static_cast<int64_t>(0x5555), static_cast<int64_t>(0x5555),
              static_cast<int64_t>(0x5555), static_cast<int64_t>(0x5555), static_cast<int64_t>(0x5555), static_cast<int64_t>(0x5555) }
        };

        static const int64_t L_order[32] = {
            0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
            0xd0, 0xce, 0x52, 0x64, 0x75, 0x4f, 0x82, 0x65,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
        };

        static void modL(uint8_t r[32], int64_t x[64])
        {
            int64_t carry, i, j;
            for (i = 63; i >= 32; --i)
            {
                carry = 0;
                for (j = i - 32; j < i - 12; ++j)
                {
                    x[j] += carry - 16 * x[i] * L_order[j - (i - 32)];
                    carry = (x[j] + 128) >> 8;
                    x[j] -= carry << 8;
                }
                x[j] += carry;
                x[i] = 0;
            }
            carry = 0;
            for (j = 0; j < 32; ++j)
            {
                x[j] += carry - (x[31] >> 4) * L_order[j];
                carry = x[j] >> 8;
                x[j] &= 255;
            }
            for (j = 0; j < 32; ++j)
            {
                x[j] -= carry * L_order[j];
                carry = x[j] >> 8;
                x[j] &= 255;
            }
            for (i = 0; i < 32; ++i)
                r[i] = static_cast<uint8_t>(x[i]);
        }

        // Verifies signature (64 bytes) over message (m, mlen) with public key pk (32 bytes)
        inline bool verify(const uint8_t sig[64], const uint8_t* m, size_t mlen, const uint8_t pk[32])
        {
            uint8_t h[64];
            uint8_t rcheck[32];
            gf p[4], q[4];

            if (unpackneg(q, pk) != 0)
                return false;

            // Compute h = SHA512(R || pk || m)
            SHA512 sha;
            sha.update(sig, 32);
            sha.update(pk, 32);
            sha.update(m, mlen);
            sha.finalize(h);

            // Reduce h modulo group order l
            int64_t x[64];
            for (int i = 0; i < 64; ++i)
                x[i] = static_cast<int64_t>(h[i]);
            uint8_t h_mod[32];
            modL(h_mod, x);

            // Compute R' = s*B + h*A
            scalarmult(p, q, h_mod);
            gf sb[4];
            scalarmult(sb, B_point, sig + 32);
            add(p, sb);

            gf t;
            inv25519(t, p[2]);
            M(p[0], p[0], t);
            M(p[1], p[1], t);
            pack25519(rcheck, p[1]);

            return (memcmp(rcheck, sig, 32) == 0);
        }
    }
}
