require('dotenv').config();
const express = require('express');
const cors = require('cors');
const axios = require('axios');

const app = express();

app.use(cors());

const CLIENT_ID = process.env.OPENSKY_CLIENT_ID;
const CLIENT_SECRET = process.env.OPENSKY_CLIENT_SECRET;

const TOKEN_URL = 'https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token';
const API_URL = 'https://opensky-network.org/api/states/all';

let accessToken = null;
let tokenExpiresAt = 0;

let cachedBuffer = Buffer.alloc(0);

async function getValidToken() {
    const now = Date.now();
    if (accessToken && now < tokenExpiresAt - 60000) return accessToken;

    const params = new URLSearchParams();
    params.append('grant_type', 'client_credentials');
    params.append('client_id', CLIENT_ID);
    params.append('client_secret', CLIENT_SECRET);

    const response = await axios.post(TOKEN_URL, params, {
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' }
    });

    accessToken = response.data.access_token;
    tokenExpiresAt = Date.now() + (response.data.expires_in || 1800) * 1000;
    return accessToken;
}

async function updateCacheFromOpenSky() {
    try {
        const token = await getValidToken();
        const response = await axios.get(API_URL, {
            headers: { 'Authorization': `Bearer ${token}` }
        });

        const states = response.data.states || [];
        const buffer = Buffer.alloc(states.length * 16);
        let offset = 0;

        for (const plane of states) {
            const lon = plane[5] ?? 0.0;
            const lat = plane[6] ?? 0.0;
            const vel = plane[9] ?? 0.0;
            const heading = plane[10] ?? 0.0;

            buffer.writeFloatLE(lat, offset);
            buffer.writeFloatLE(lon, offset + 4);
            buffer.writeFloatLE(vel, offset + 8);
            buffer.writeFloatLE(heading, offset + 12);
            offset += 16;
        }

        cachedBuffer = buffer;
        console.log(`[Cache Updated] ${new Date().toLocaleTimeString()} - ${states.length} samolotów (${(buffer.length / 1024).toFixed(1)} KB) w RAM`);

    } catch (error) {
        console.error('[OpenSky Error]', error.response?.status || error.message);
    }
}

setInterval(updateCacheFromOpenSky, 120000);
updateCacheFromOpenSky();

app.get('/api/planes', (req, res) => {
    res.setHeader('Content-Type', 'application/octet-stream');
    res.send(cachedBuffer);
});

app.listen(8080, '0.0.0.0', () => console.log('Serwer działa pod http://127.0.0.1:8080'));