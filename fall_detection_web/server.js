// server.js
const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const path = require('path');
const fs = require('fs')
const nodemailer = require('nodemailer');
const webpush = require('web-push');

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

const PORT = process.env.PORT || 3000;

const mailerEnabled = true;
const transporter = nodemailer.createTransport({
    service: 'gmail',
    auth:{
        user: 'yahyabachtiar03@gmail.com',
        pass: 'ivansyah'
    }
});

const emailPenerimaNotifikasi = 'comvisvtol@gmail.com';

const VAPID_PUBLIC_KEY = 'BEewNteJXMHS2cmbrKT5yPGZgjZgtxKRTdm0aNavR5xRs2sxXCM9fVIbzGSRlOJ60bbjibEFsh97jIPLrmcksdU';
const VAPID_PRIVATE_KEY = 'KoUg_OaQ02x8bx_fXp11wAu8rFvg8FUWUGU3mztD7N8';

if (!VAPID_PUBLIC_KEY || !VAPID_PRIVATE_KEY || VAPID_PUBLIC_KEY === 'GANTI DENGAN PUBLIC_KEY ANDA'){
    console.error("VAPID keys belum diatur dengan benar di server.js. Hasilkan menggunakan 'npx web-push generate-vapid-keys' dan masukkan ke kode.");
    process.exit(1);
} else {
    webpush.setVapidDetails(
        'mailto: comvisvtol@gmail.com',
        VAPID_PUBLIC_KEY,
        VAPID_PRIVATE_KEY
    );
}

// Middleware untuk menyajikan file statis dari folder 'public'
app.use(express.static(path.join(__dirname, 'public')));
app.use(express.json()); // Untuk parsing body JSON jika ESP32 mengirim POST dengan JSON

//Array untuk menyimpan log kejadian
let fallEventsLog = [];
let pushSubscriptions = [];
let deviceStates = {};

//Fungsi untuk mencatat kejadian ke file
function logFallEventToFile(eventData){
    const logEntry = `[${new Date(eventData.timestamp).toISOString()}] Device: ${eventData.deviceId || 'Unknown'}, Location: ${eventData.location || 'N/A'}, IP: ${eventData.ip}\n`;
    fs.appendFile('fall_events.log', logEntry, (err) => {
        if (err) console.error('Gagal menulis ke log file: ', err);
    });
}

// Fungsi untuk mengirim email notifikasi (opsional)
async function sendEMailNotification(eventData){
    if (!mailerEnabled) return;
    const mailOptions = {
        from:'"Sistem Pendeteksi Jatuh" <yahyabachtiar03@gmail.com>',
        to: emailPenerimaNotifikasi,
        subject: 'PEMBERITAHUAN: Terdeteksi Jatuh!',
        text: `Peringatan! Terdeteksi potensi jatuh.\n\nID Perangkat: ${eventData.deviceId || 'Unknown'}\nLokalsi: ${eventData.location || 'N/A'}\nWaktu: ${new Date(eventData.timestamp).toLocaleString()}\nIP ESP32: ${eventData.ip}\n\nSegera periksa kondisi pengguna.`,
        html: `<p>Peringatan! Terdeteksi potensi jatuh.</p>
               <p><strong>ID Perangkat:</strong> ${eventData.deviceId || 'Unknown'}<br>
               <strong>Lokasi:</strong> ${eventData.location || 'N/A'}<br>
               <strong>Waktu:</strong> ${new Date(eventData.timestamp).toLocaleString()}<br>
               <strong>IP ESP32:</strong> ${eventData.ip}</p>
               <p>Segera periksa kondisi pengguna.</p>`
    };

    try {
        let info = await transporter.sendMail(mailOptions);
        console.log('Email notifikasi terkirim: %s', info.messageId);
    } catch (error) {
        console.error('Gagal mengirim email: ', error);
    }
}

// ... (setelah fungsi sendEmailNotification)

// TAMBAHKAN FUNGSI BARU INI
async function handleFallAlert(fallData) {
    // 1. Catat ke riwayat untuk ditampilkan di web
    fallEventsLog.push(fallData);
    if (fallEventsLog.length > 50) fallEventsLog.shift();

    // 2. Siarkan kejadian jatuh ke frontend (untuk alert merah)
    broadcastToFrontends({ type: 'fall_detected', data: fallData });

    // 3. Catat ke file log
    logFallEventToFile(fallData);

    // 4. Kirim Notifikasi Email
    if (mailerEnabled) await sendEmailNotification(fallData);
    
    // 5. Kirim Notifikasi Push Browser (Logika push notif Anda)
    if (VAPID_PUBLIC_KEY !== 'GANTI DENGAN PUBLIC_KEY ANDA') {
        const notificationPayload = JSON.stringify({
            title: "PERHATIAN: Jatuh Terdeteksi!",
            body: `Perangkat: ${fallData.deviceId} di ${fallData.location || 'N/A'}. Segera periksa!`,
            icon: '/icons/icon-192x192.png',
            data: { url: '/', timestamp: fallData.timestamp }
        });

        const sendPromises = pushSubscriptions.map(subscription => 
            webpush.sendNotification(subscription, notificationPayload)
                .catch(error => {
                    if (error.statusCode === 404 || error.statusCode === 410) {
                        pushSubscriptions = pushSubscriptions.filter(s => s.endpoint !== subscription.endpoint);
                    }
                })
        );
        await Promise.allSettled(sendPromises);
    }
}

// GANTI FUNGSI LAMA ANDA DENGAN INI
function broadcastToFrontends(message) {
    const messageString = JSON.stringify(message);
    wss.clients.forEach((client) => {
        // DIUBAH: Ditambahkan "&& !client.isDevice"
        if (client.readyState === WebSocket.OPEN && !client.isDevice) {
            client.send(messageString);
        }
    });
}

//WebSocket connection handling
wss.on('connection', (ws) => {
    console.log('Klien frontend terhubung via WebSocket');

    
    ws.on('message', (message) => {
        let data;
        try {
            data = JSON.parse(message);
        } catch (e) {
            console.error('Menerima pesan tidak valid:', message);
            return;
        }

        if (data.type === 'state_update' && data.deviceId) {
            ws.isDevice = true; // Tandai koneksi ini sebagai perangkat ESP32
            
            console.log(`Update status dari ${data.deviceId}: ${data.state}`);
            deviceStates[data.deviceId] = { state: data.state, lastUpdate: Date.now() };
            
            // Siarkan status baru ini ke semua frontend
            broadcastToFrontends({
                type: 'state_update',
                data: {
                    deviceId: data.deviceId,
                    state: data.state
                }
            });
            
            // Jika statusnya adalah alarm terpicu, panggil handleFallAlert
            if (data.state === 'ALARM_TRIGGERED') {
                const fallData = {
                    deviceId: data.deviceId,
                    location: 'Lokasi dari ESP',
                    timestamp: Date.now(),
                    ip: ws._socket.remoteAddress
                };
                console.log(`JATUH TERKONFIRMASI dari ${data.deviceId}. Memicu notifikasi...`);
                handleFallAlert(fallData);
            }
        }
    });
    
    ws.on('close', ()=>{
        console.log('Klien frontend terputus');
    });
    
    ws.on('error', (error) => {
        console.error('WebSocket error:', error);
    });

    ws.send(JSON.stringify({type: 'history', data: fallEventsLog}));
    ws.send(JSON.stringify({ type: 'initial_states', data: deviceStates }));
});

// function broadcastFallNotification(eventData){
//     fallEventsLog.push(eventData);
//     if (fallEventsLog.length > 50){
//         fallEventsLog.shift();
//     }

//     const message = JSON.stringify({ type: 'fall_detected', data: eventData});
//     wss.clients.forEach((client) => {
//         if (client.readyState === WebSocket.OPEN){
//             client.send(message);
//         }
//     })
// }

// Endpoint untuk mengirim VAPID public key ke frontend
app.get('/api/vapid-public-key', (req, res) => {
    res.send(VAPID_PUBLIC_KEY);
});
// Endpoint untuk menyimpan langganan push dari frontend
app.post('/api/subscribe', (req, res) => {
    const subscription = req.body;
    if (!subscription || !subscription.endpoint) {
        return res.status(400).json({error: 'Data langganan tidak valid.'});
    }

    // Cek duplikasi berdasarkan endpoint
    const existingSubscription = pushSubscriptions.find(sub => sub.endpoint === subscription.endpoint);
    if (existingSubscription){
        console.log('Langganan push sudah ada: ', subscription.endpoint);
        console.log('Total langganan: ', pushSubscriptions.length);
    }
    res.status(201).json({ message: 'Langganan berhasil disimpan.'});
});

// Endpoint untuk menghapus langganan push (opsional)
app.post('/api/unsubscribe', (req, res) => {
    const { endpoint } = req.body;
    if (!endpoint) {
        return res.status(400).json({ error: 'Endpoint diperlukan untuk unsubscribe.' });
    }
    const initialLength = pushSubscriptions.length;
    pushSubscriptions = pushSubscriptions.filter(sub => sub.endpoint !== endpoint);
    if (pushSubscriptions.length < initialLength) {
        console.log('Langganan push dihapus:', endpoint);
    } else {
        console.log('Langganan push tidak ditemukan untuk dihapus:', endpoint);
    }
    res.status(200).json({ message: 'Proses unsubscribe selesai.' });
});

// --- TITIK INTEGRASI ESP32 ---
// ESP32 akan mengirim permintaan ke endpoint ini.
// Bisa menggunakan GET atau POST. POST lebih disarankan jika mengirim data.

// app.all('/api/fall-detected', async (req, res) => {
//     console.log('Menerima trigger dari ESP32...');
//     const deviceId = req.query.deviceId || req.body.deviceId || 'ESP32_C6_Default';
//     const location = req.query.location || req.body.location || 'Lokasi Tidak Diketahui';
//     const timestamp = Date.now();
//     const clientIp = req.ip || req.connection.remoteAddress;

//     const fallData = {
//         deviceId: deviceId,
//         location: location,
//         timestamp: timestamp,
//         ip: clientIp,
//         title: "PERHATIAN: Jatuh Terdeteksi!",
//         body: `Perangkat: ${deviceId} di ${location || 'N/A'}. Segera periksa!`, // Isi notifikasi push
//         icon: '/icons/icon-192x192.png',
//         url: '/' // URL yang akan dibuka ketika notifikasi di-klik
//     };

//     console.log('Data Jatuh Terdeteksi: ', fallData);

//     // 1. Broadcast ke klien WebSocket
//     broadcastFallNotification(fallData);

//     // 2. Catat file log
//     logFallEventToFile(fallData);

//     // 3. Kirim Notifikasi Email
//     if (mailerEnabled) await sendEmailNotification(fallData);

//     // 4. Kirim Notifikasi Push Browser ke semua subscriber
//     if(VAPID_PUBLIC_KEY !== 'BEewNteJXMHS2cmbrKT5yPGZgjZgtxKRTdm0aNavR5xRs2sxXCM9fVIbzGSRlOJ60bbjibEFsh97jIPLrmcksdU') {
//         console.log(`Mencoba mengirim notifikasi push ke ${pushSubscriptions.length} subscriber...`);
//         const notificationPayload = JSON.stringify({
//             title: fallData.title,
//             body: fallData.body,
//             icon: fallData.icon,
//             data: { url: fallData.url, timestamp: fallData.timestamp}
//         });

//         const sendPromises = pushSubscriptions.map(subscription => webpush.sendNotification(subscription, notificationPayload)
//             .then(response => {
//                 console.log(`Notifikasi push terkirim ke: ${subscription.endpoint.substring(0,50)} ...Status: ${response.statusCode}`);
//             })  
//             .catch(error => {
//             console.error(`Gagal mengirim notifikasi push ke: ${subscription.endpoint.substring(0, 50)} ... Status: ${error.statusCode}`);
//                     // Jika endpoint tidak valid lagi (misal, 404 atau 410), hapus dari daftar
//                     if (error.statusCode === 404 || error.statusCode === 410) {
//                         console.log('Menghapus langganan tidak valid:', subscription.endpoint);
//                         pushSubscriptions = pushSubscriptions.filter(s => s.endpoint !== subscription.endpoint);
//                     }
//                 })
//         );
//         await Promise.allSettled(sendPromises);
//     }   
//     res.status(200).json({ message: 'Notifikasi diterima dan diproses (termasuk push jika ada subscriber)', data: fallData });
// });

// Endpoint untuk mendapatkan log (jika diperlukan oleh frontend secara eksplisit)
app.get('/api/get-logs', (req, res) => {
    res.json(fallEventsLog);
});


// Jalankan server
server.listen(PORT, '0.0.0.0', () => {
    console.log(`Server berjalan di port ${PORT}`);
    console.log('ESP32 harus terhubung ke WebSocket publik dari server ini');
    console.log(`Frontend dapat diakses setelah deployment`);
});

// Jalankan Local
// server.listen(PORT, () => {
//     console.log(`Server berjalan di http://localhost:${PORT}`);
//     console.log(`ESP32 harus terhubung ke WebSocket di: ws://<IP_SERVER_ANDA>:${PORT}`);
// });
