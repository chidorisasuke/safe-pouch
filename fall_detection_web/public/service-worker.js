// public/service-worker.js
console.log('[Service Worker] File dimuat.');

self.addEventListener('install', event => {
    console.log('[Service Worker] Proses install...');
    // event.waitUntil(self.skipWaiting()); // Aktifkan SW baru segera (opsional)
});

self.addEventListener('activate', event => {
    console.log('[Service Worker] Diaktifkan.');
    // event.waitUntil(clients.claim()); // Ambil kontrol halaman yang terbuka segera (opsional)
});

self.addEventListener('push', event => {
    console.log('[Service Worker] Notifikasi Push diterima.');

    let data = {};
    if (event.data) {
        try {
            data = event.data.json();
        } catch (e) {
            console.error("Error parsing data push:", e);
            data = { title: "Notifikasi Baru", body: event.data.text() };
        }
    } else {
        data = { title: "Notifikasi Sistem", body: "Ada pembaruan penting." };
    }

    const title = data.title || 'Pemberitahuan Sistem Jatuh';
    const options = {
        body: data.body || 'Sebuah kejadian telah terdeteksi.',
        icon: data.icon || '/icons/icon-192x192.png', // Pastikan icon ada di folder public/icons
        badge: data.badge || '/icons/badge-72x72.png', // Opsional, untuk Android
        vibrate: [100, 50, 100], // Pola getar [getar, jeda, getar]
        data: { // Data yang bisa diakses saat notifikasi di-klik
            url: data.url || '/', // URL default jika tidak ada
            timestamp: data.timestamp || Date.now()
        },
        // tag: 'fall-notification' // Opsional: notifikasi dengan tag yang sama akan menggantikan yang lama
    };

    event.waitUntil(
        self.registration.showNotification(title, options)
    );
});

self.addEventListener('notificationclick', event => {
    console.log('[Service Worker] Notifikasi di-klik.');
    event.notification.close(); // Tutup notifikasi yang di-klik

    const urlToOpen = event.notification.data.url || '/';

    // Coba buka tab yang sudah ada atau buka tab baru
    event.waitUntil(
        clients.matchAll({ type: 'window', includeUncontrolled: true }).then(windowClients => {
            let matchingClient = null;
            for (let i = 0; i < windowClients.length; i++) {
                const client = windowClients[i];
                // Cek apakah URL sama (abaikan query string atau hash jika perlu)
                if (new URL(client.url).pathname === new URL(urlToOpen, self.location.origin).pathname) {
                    matchingClient = client;
                    break;
                }
            }

            if (matchingClient) {
                return matchingClient.focus(); // Fokus ke tab yang sudah ada
            } else if (clients.openWindow) {
                return clients.openWindow(urlToOpen); // Buka tab baru
            }
        })
    );
});