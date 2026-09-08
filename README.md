# GeceDPI Turkey - Linux

> Türkiye'de ISP tarafından engellenen sitelere (Discord, YouTube vb.) **VPN'siz** erişim sağlayan DPI geçiş aracı.
> VPN-less DPI bypass for Turkey ISPs. Linux (Arch / CachyOS / systemd tabanlı dağıtımlar).

---

## Özellikler

- **Otomatik mod (`--auto`)** — Başlangıçta 7 farklı DPI geçiş tekniğini dener (SuperOnline, Türk Telekom, Vodafone ve diğer ISP yöntemleri dahil) ve bağlantında **çalışan en hızlı bypass'ı seçer**. Elini hiçbir şeye değdirmeden çalışır.
- **DNS koruması** — DNS sorgularını nftables ile **Cloudflare (1.1.1.1)** üzerinden yönlendirir; ISP'nin DNS engellemesini/yönlendirmesini aşar.
- **SNI-DPI geçişi** — TLS ClientHello'yu (SNI dahil) küçük parçalara bölüp sahte paketlerle karıştırır; DPI imzayı okuyamaz.
- **systemd servisi** — Açılışta otomatik başlar, çökerse kendini yeniden test edip kaldığı yerden devam eder.
- Tüm Türkiye ISP'leri (Türk Telekom, Turkcell, Vodafone, SuperOnline) için çalışma hedeflidir.

---

## Nasıl Çalışır

| Windows DPI araçları | GeceDPI (Linux) |
|---|---|
| WinDivert (kernel driver) | **NFQUEUE** (libnetfilter_queue) |
| WinDivert filter string | **nftables** kuralları |
| Windows Service | **systemd** servisi |
| DNS redirect (userspace) | **nftables DNAT** (port 53 → Cloudflare:53) |
| Fake packet injection | **raw socket** (SOCK_RAW + IP_HDRINCL) |

**Paket akışı:**
1. DNS sorguları → nftables DNAT → Cloudflare DNS `1.1.1.1:53`
2. HTTPS trafiği → NFQUEUE → `gecedpi` daemon
3. Daemon ilk TLS paketini böler + sahte paket gönderir → DPI karışır → bağlantı geçer

---

## Gereksinimler

- Linux (Arch, CachyOS, Manjaro veya systemd kullanan herhangi bir dağıtım)
- `libnetfilter_queue`, `nftables`, `gcc`, `make`
- Root/sudo yetkisi

Arch tabanlı sistemlerde:
```bash
sudo pacman -S libnetfilter_queue nftables gcc make
```

---

## Kurulum

### Yöntem 1: Sıfırdan (tek blok, tavsiye edilen)

```bash
sudo pacman -S libnetfilter_queue nftables gcc make
git clone https://github.com/gecedabest/gecedpi
cd gecedpi
make
sudo bash service_install_dnsredir_turkey.sh
```


### Yöntem 2: Release binary

1. [Releases](https://github.com/gecedabest/Gecedpi---Goodbyedpi-for-Linux-Arch-CachyOS-) sayfasından `gecedpi-linux-x86_64` indirin.
2. Yerleştirip kurun:
```bash
git clone https://github.com/gecedabest/gecedpi
cd gecedpi
mkdir -p bin
mv ~/Downloads/gecedpi-linux-x86_64 bin/gecedpi
chmod +x bin/gecedpi
sudo bash service_install_dnsredir_turkey.sh
```

### Kurulum sırasında ne olur?

Kurulum scripti şunları yapar:
- `compile` eder ve `bin/gecedpi` binary'sini hazırlar (yoksa)
- nft kural dosyasını `/usr/local/sbin/gecedpi-nft-load.sh` olarak kurar
- systemd servisini oluşturup **enable + start** eder (`/etc/systemd/system/gecedpi.service`)
- DNS'i Cloudflare'e yönlendirir (`/etc/resolv.conf` → `1.1.1.1` / `1.0.0.1`)
- Discord erişimini test edip özet döker

---

## Otomatik Mod (`--auto`)

`--auto` açıkken gecedpi, başlangıçta `discord.com:443`'e bağlanmayı **7 farklı teknikle** dener ve en hızlı çalışanı seçer:

| Aday mod | Fragment | TTL | Fake paket |
|---|---|---|---|
| classic frag5 ttl5 fake | 5 bayt | 5 | açık |
| nofrag ttl3 fake | kapalı | 3 | açık |
| frag9 ttl5 fake | 9 bayt | 5 | açık |
| frag5 ttl5 nofake | 5 bayt | 5 | kapalı |
| frag2 ttl5 fake | 2 bayt | 5 | açık |
| frag1 ttl5 fake | 1 bayt | 5 | açık |
| frag1 ttl3 fake | 1 bayt | 3 | açık |

- Sadece **başlangıçta** test edilir; çalışırken mod değişmez.
- **Ağ değiştirdiyseniz** (farklı ISP/WiFi): `sudo systemctl restart gecedpi` — yeniden test eder.
- Hangi mod seçildiğini görmek için: `journalctl -u gecedpi -f`

Örnek çıktı:
```
[...] [gecedpi] auto: classic frag5 ttl5 fake works (29 ms)
[...] [gecedpi] auto: frag2 ttl5 fake works (35 ms)
[...] [gecedpi] auto: selected classic frag5 ttl5 fake (29 ms)
```

Hiçbir mod çalışmazsa otomatik olarak `classic frag5 ttl5 fake`'e döner (her durumda en az bir mod aktif kalır).

---

## Günlük Kullanım

```bash
# Durum
sudo systemctl status gecedpi

# Yeniden başlatma, ağ değişir veya sadece istiyorsanız.
sudo systemctl restart gecedpi

# Geçici durdur, sonraki bootta tekrar açılır, istemiyorsanız disable versionunu kullanın.
sudo systemctl stop gecedpi

# Logları canlı izle
journalctl -u gecedpi -f

# Aktif nftables kuralları
sudo nft list table inet gecedpi
```

**Tek seferlik çalıştırma** (servis kurmadan dene):
```bash
sudo bash turkey_dnsredir.sh
```

---

## Doğrulama

Kurulum sonrası her şeyin yolunda olduğunu kontrol edin:

```bash
# 1) DNS Cloudflare'den mi çözülüyor? (162.159.x.x GERÇEK IP olmalı)
dig discord.com +short

# 2) HTTPS erişimi (HTTP 200 beklenir)
curl -o /dev/null -w "%{http_code}\n" https://discord.com

# 3) nft kuralları yüklü mü?
sudo nft list table inet gecedpi

# 4) Servis aktif mi, hangi mod seçildi?
systemctl is-active gecedpi
journalctl -u gecedpi --no-pager | grep -E "selected|auto:"
```

> `dig discord.com +short` sonucu `162.159.x.x` değil de `195.175.x.x` gibi bir IP dönerse ISP DNS yönlendirmesi hâlâ aktif demektir → `sudo systemctl restart gecedpi` deneyin.

---

## Hangi Script'i Kullanmalıyım?

# UYARI NOTU; BU VERSIYON ZATEN OTOMATIK OLARAK BYPASSLEYENLERI TOPLAYIP EN HIZLI GECENI SECMEKTEDIR, BU KISIM ELLE YAPMAK ISTEYENLER İÇİN GEÇERLİDİR.

| Script | Açıklama | ISP |
|--------|----------|-----|
| `service_install_dnsredir_turkey.sh` | **Ana** - Otomatik mod + DNS Cloudflare | Tüm ISP'ler |
| `turkey_dnsredir.sh` | Servissiz, tek seferlik (terminal kapanınca durur) | Tüm ISP'ler |
| `service_install_dnsredir_turkey_alternative_superonline.sh` | TTL=3, DNS manuel | SuperOnline |
| `service_install_dnsredir_turkey_alternative2_superonline.sh` | Fragment=5, DNS manuel | SuperOnline |
| `service_install_dnsredir_turkey_alternative3_superonline.sh` | TTL=3 + DNS Cloudflare | SuperOnline |
| `service_install_dnsredir_turkey_alternative4_superonline.sh` | Fragment=5 + DNS Cloudflare | SuperOnline |
| `service_install_dnsredir_turkey_alternative5_superonline.sh` | Fragment=9 + TTL(5) + DNS Cloudflare | SuperOnline |
| `service_install_dnsredir_turkey_alternative6_superonline.sh` | Fragment=9, DNS manuel | SuperOnline |
| `service_remove.sh` | **Kaldır** - Tüm kurallar + servis + DNS geri alma | — |

> **DNS manuel** yazanlar için: Ağ ayarlarından DNS'i `1.1.1.1` veya `1.0.0.1` yapın.

---

## Sorun Giderme

**Servis başlamıyor:**
```bash
journalctl -u gecedpi -n 30
cat /tmp/gecedpi-nft.log
```

**Discord hâlâ açılmıyor:**
1. DNS kontrolü: `dig discord.com +short` → `162.159.x.x` olmalı
2. Yabancı ağ / kafe / kurumsal WiFi kullanıyorsanız senaryo farklı: bazı ağlar SNI yerine **IP/port engeli** uygular — hiçbir DPI modu o ağda işlemez, orada yalnızca VPN çözüm olur
3. Tarayıcı üzerinden deniyorsanız QUIC'i kapatın: `chrome://flags/#enable-quic` → Disabled

**nftables kuralları yüklenmedi:**
```bash
sudo nft -f /home/$USER/gecedpi/nftables/gecedpi.nft
sudo systemctl restart gecedpi
```

---

## Kaldırma

```bash
sudo bash service_remove.sh
```
Servisi durdurur/disable eder, nft kurallarını siler, DNS'i sistem varsayılanına geri alır (`resolv.conf` → systemd-resolved stub). `bin/gecedpi` ve kaynak dosyalar repo klasöründe kalır.

---

## Binary Parametreleri

```
--fragment-size N    TCP parçalama boyutu (varsayılan: 5)
--no-fragment        Parçalamayı devre dışı bırak (sadece sahte paket)
--set-ttl N          Sahte paket TTL değeri (varsayılan: 5)
--no-fake            Sahte paket göndermeyi devre dışı bırak
--dns-addr ADDR      DNS DNAT hedef adresi
--dns-port PORT      DNS DNAT hedef portu (varsayılan: 53)
--dnsv6-addr ADDR    IPv6 DNS adresi
--dnsv6-port PORT    IPv6 DNS portu
--queue-num N        NFQUEUE numarası (varsayılan: 0)
--auto               Başlangıçta otomatik mod seçimi
```

---

## Teknik Detaylar

**Kaynak dosyalar:**
```
src/
├── main.c/h        - Ana program, argüman parsing
├── packet.c/h      - Ham IP/TCP/UDP paket parse + checksum
├── conntrack.c/h   - Bağlantı takibi (uthash, sadece 1. paket işlenir)
├── fakepackets.c/h - Sahte TLS ClientHello byte dizileri
├── fragment.c/h    - TCP payload fragmantasyonu
├── nfqueue.c/h     - NFQUEUE event loop
├── autotune.c/h    - Otomatik mod test motoru (7 aday, en hızlı seçim)
└── utils/uthash.h  - Header-only hash table
```

**Manuel derleme:**
```bash
cd gecedpi
make            # derle (bin/gecedpi)
make install    # bin/ klasörüne kopyala
make debug      # debug modda derle
make clean      # temizle
```

---

## Teşekkür/Credits.
salihkahveci090 -- linux versiyonunu gördüğüm adam.
cagritaskn -- goodbyedpi projesini ülkemize göre ayarlayıp olayını türkiyeye taşıyan imparator

Açık kaynak topluluğundaki tüm DPI çalışmalarına teşekkür ederiz. Bu proje, Linux/NFQUEUE tabanlı DPI geçiş yaklaşımını sistematik olarak uygular.

---

## Lisans / License

MIT License — Özgürce kullanabilir, değiştirebilir ve dağıtabilir.
