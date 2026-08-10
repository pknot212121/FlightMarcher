require('dotenv').config();

const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const axios = require('axios');

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

const CLIENT_ID = process.env.OPENSKY_CLIENT_ID;
const CLIENT_SECRET = process.env.OPENSKY_CLIENT_SECRET;

const TOKEN_URL = 'https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token';
const API_URL = 'https://opensky-network.org/api/states/all';

let accessToken = null;
let tokenExpiresAt = 0;

async function getValidToken() {
    const now = Date.now();

    if (accessToken && now < tokenExpiresAt - 60000) {
        return accessToken;
    }

    console.log('[Auth] Pobieranie nowego tokenu OAuth2 z OpenSky...');

    const params = new URLSearchParams();
    params.append('grant_type', 'client_credentials');
    params.append('client_id', CLIENT_ID);
    params.append('client_secret', CLIENT_SECRET);

    try {
        const response = await axios.post(TOKEN_URL, params, {
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' }
        });

        accessToken = response.data.access_token;
        const expiresInMs = (response.data.expires_in || 1800) * 1000;
        tokenExpiresAt = Date.now() + expiresInMs;

        console.log('[Auth] Nowy token pobrany pomyślnie!');
        return accessToken;
    } catch (error) {
        console.error('[Auth Błąd]', error.response?.data || error.message);
        throw error;
    }
}

let cachedBuffer = null;

async function fetchOpenSkyData() {
    try {
        const token = await getValidToken();
        const response = await axios.get(API_URL, {
            headers: {
                'Authorization': `Bearer ${token}`
            }
        });

        const states = response.data.states || [];

        const buffer = Buffer.alloc(states.length * 16);
        let offset = 0;

        for (const plane of states) {
            const lon = plane[5] ?? 0.0;
            const lat = plane[6] ?? 0.0;
            const alt = plane[7] ?? plane[13] ?? 0.0;
            const heading = plane[10] ?? 0.0;

            buffer.writeFloatLE(lat, offset);
            buffer.writeFloatLE(lon, offset + 4);
            buffer.writeFloatLE(alt, offset + 8);
            buffer.writeFloatLE(heading, offset + 12);

            offset += 16;
        }

        cachedBuffer = buffer;

        wss.clients.forEach(client => {
            if (client.readyState === WebSocket.OPEN) {
                client.send(cachedBuffer);
            }
        });

        console.log(`[API] Zaktualizowano: ${states.length} samolotów (${(buffer.length / 1024).toFixed(1)} KB)`);

    } catch (error) {
        console.error('[API Błąd]', error.response?.status, error.message);
    }
}

setInterval(fetchOpenSkyData, 10000);

wss.on('connection', (ws) => {
    if (cachedBuffer) {
        ws.send(cachedBuffer);
    }
});

server.listen(8080, () => console.log('Serwer proxy uruchomiony na porcie 8080'));