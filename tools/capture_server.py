#!/usr/bin/env python3
"""Local phone capture companion. Stdlib only; validates with the real game.

python3 tools/capture_server.py [--host 0.0.0.0] [--port 8765]
Use --cert certificate.pem --key key.pem for live cameras over trusted HTTPS.
"""
import argparse
import base64
import binascii
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import socket
import ssl
import struct
import subprocess
import tempfile
import threading
import uuid
import zlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit

ROOT = Path(__file__).resolve().parents[1]
STATIC = ROOT / "tools" / "capture"
MAX_REQUEST = 160 * 1024 * 1024
POSES = [
    ("stand", "Готовность", "Поставьте бойца боком вправо. Ноги на линии пола, руки перед собой."),
    ("breathe", "Вдох", "Чуть приподнимите грудь и руки. Стопы остаются на месте."),
    ("walk1", "Шаг · вперёд", "Правая нога впереди, левая сзади. Голова смотрит вправо."),
    ("walk2", "Шаг · перенос веса", "Сведите ноги под телом и немного приподнимите заднюю стопу."),
    ("walk3", "Шаг · другая нога", "Теперь левая нога впереди, правая сзади."),
    ("walk4", "Шаг · завершение", "Перенесите вес обратно. Это последний кадр цикла ходьбы."),
    ("punch1", "Удар · замах", "Отведите переднюю руку назад. Весь кулак должен остаться в кадре."),
    ("punch2", "Удар · контакт", "Вытяните руку вправо. Не двигайте камеру ближе к бойцу."),
    ("kick", "Удар ногой", "Одну стопу вытяните вправо, другую оставьте на линии пола."),
    ("crouch", "Приседание", "Согните ноги и опустите голову. Размер бойца не меняется."),
    ("jump", "Прыжок", "Подтяните ноги. Снимите позу на столе: высоту прыжка добавит игра."),
    ("block", "Защита", "Закройте голову руками, немного отклонитесь назад."),
    ("hit", "Получает удар", "Отклоните голову и корпус назад, руки разведите."),
    ("down", "Падение", "Положите бойца на бок. Не приближайте камеру: оставьте место для всего тела."),
    ("win", "Победа", "Поднимите обе руки. Не обрежьте пальцы и голову."),
    ("portrait", "Портрет", "Теперь приблизьте камеру. Голова и плечи крупно, лицо к зрителю."),
]
IDS = [p[0] for p in POSES]


def blob(value, allowed, limit=12 * 1024 * 1024):
    if not isinstance(value, str) or "," not in value:
        raise ValueError("Не удалось прочитать файл")
    header, encoded = value.split(",", 1)
    mime = header.removeprefix("data:").split(";")[0]
    if mime not in allowed or ";base64" not in header:
        raise ValueError("Неподдерживаемый формат файла")
    try:
        data = base64.b64decode(encoded, validate=True)
    except (ValueError, binascii.Error) as e:
        raise ValueError("Повреждённый файл") from e
    if not data or len(data) > limit:
        raise ValueError("Файл пустой или слишком большой")
    return mime, data


def png(value):
    _, data = blob(value, {"image/png"}, 3 * 1024 * 1024)
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("Ожидался PNG")
    pos, payload, expected, ended = 8, bytearray(), None, False
    while pos + 12 <= len(data):
        n, = struct.unpack_from(">I", data, pos)
        kind = data[pos + 4:pos + 8]
        if pos + n + 12 > len(data):
            raise ValueError("Обрезанный PNG")
        content = data[pos + 8:pos + n + 8]
        crc, = struct.unpack_from(">I", data, pos + n + 8)
        if zlib.crc32(kind + content) & 0xffffffff != crc:
            raise ValueError("Повреждённый PNG")
        if kind == b"IHDR":
            if expected is not None or n != 13:
                raise ValueError("Некорректный PNG")
            w, h, depth, color, comp, filt, interlace = struct.unpack(">IIBBBBB", content)
            if (w, h) != (640, 480) or depth != 8 or color not in (2, 6) or comp or filt or interlace:
                raise ValueError("Кадры должны быть 640×480, 8-bit RGB/RGBA PNG")
            expected = h * (1 + w * (4 if color == 6 else 3))
        elif kind == b"IDAT":
            payload.extend(content)
        elif kind == b"IEND":
            ended = True
            break
        pos += n + 12
    if expected is None or not ended:
        raise ValueError("Неполный PNG")
    try:
        decoder = zlib.decompressobj()
        raw = decoder.decompress(bytes(payload), expected + 1)
    except zlib.error as e:
        raise ValueError("Повреждённые пиксели PNG") from e
    if len(raw) != expected or not decoder.eof or decoder.unused_data:
        raise ValueError("Неверный размер пикселей PNG")
    return data


def creature_text(name):
    lines = [f"name {name}", "art photo right", "height 92", "portrait photos/portrait.png",
             "speed 1.4 1.1 7.5", "width 26", ""]
    def anim(key, frames, ticks=8, mode=""):
        lines.append(f"anim {key} {mode}".rstrip())
        lines.extend(f"  photos/{frame}.png {ticks}" for frame in frames)
        lines.extend(["end", ""])
    anim("stand", ["stand", "breathe"], 12, "loop")
    anim("walkf", ["walk1", "walk2", "walk3", "walk4"], 7, "loop")
    anim("walkb", ["walk4", "walk3", "walk2", "walk1"], 8, "loop")
    for key, frame in [("crouch", "crouch"), ("jump", "jump"), ("land", "crouch"),
                       ("block", "block"), ("crouchblock", "block"), ("hit", "hit"),
                       ("crouchhit", "hit"), ("hithard", "hit"), ("fall", "down"),
                       ("down", "down"), ("getup", "crouch"), ("dizzy", "hit"), ("win", "win")]:
        anim(key, [frame], 12, "hold" if key in ("crouch", "block", "crouchblock", "down", "win") else "")
    anim("punch", ["stand", "punch1", "punch2", "stand"], 6)
    anim("kick", ["stand", "kick", "kick", "stand"], 6)
    for group in ("stand", "crouch", "jump", "air"):
        for button, action, damage, level in [("hq", "punch", 6, "high"), ("hf", "punch", 11, "high"),
                                               ("lq", "kick", 7, "low"), ("lf", "kick", 11, "low")]:
            lines.extend([f"move {group}.{button} {action} damage {damage} stun 18 strength 2 level {level}", "end", ""])
    return "\n".join(lines)


class CaptureStore:
    def __init__(self, creatures, data, executable):
        self.creatures = Path(creatures).resolve()
        self.data = Path(data).resolve()
        self.executable = Path(executable).resolve()
        self.lock = threading.Lock()

    def publish(self, request):
        raw_name = request.get("name", "")
        if not isinstance(raw_name, str): raise ValueError("Введите имя бойца")
        name = raw_name.strip().upper()
        if not re.fullmatch(r"[A-Z][A-Z0-9_]{0,15}", name):
            raise ValueError("Имя: 1–16 латинских букв, цифр или _, начинается с буквы")
        frames = request.get("frames", {})
        if not isinstance(frames, dict) or set(frames) != set(IDS):
            raise ValueError("Нужны все 15 поз и отдельный портрет")
        mode = request.get("mode", "tripod")
        if mode not in ("tripod", "handheld"):
            raise ValueError("Неизвестный режим съёмки")
        # Decode everything before writing. The fixed key set cannot introduce paths.
        images, sources, videos, adjustments = {}, {}, {}, {}
        for key in IDS:
            frame = frames[key]
            if not isinstance(frame, dict):
                raise ValueError("Некорректный кадр")
            images[key] = png(frame.get("image"))
            if frame.get("source"):
                sources[key] = blob(frame["source"], {"image/png", "image/jpeg"})
            if frame.get("video"):
                videos[key] = blob(frame["video"], {"video/webm", "video/mp4", "video/quicktime"})
            adjustments[key] = frame.get("alignment", {})
        with self.lock:
            self.creatures.mkdir(parents=True, exist_ok=True)
            paths = list(self.creatures.glob("*.txt")) + list(self.creatures.glob("*/*.txt"))
            if len(paths) >= 64:
                raise ValueError("Список бойцов заполнен (64)")
            for path in paths:
                if re.search(r"^name\s+" + re.escape(name) + r"\s*$", path.read_text(), re.M | re.I):
                    raise ValueError("Такое имя уже есть. Выберите другое")
            # Hidden staging beside destination: atomic rename on the same filesystem.
            staging = Path(tempfile.mkdtemp(prefix=".capture-", dir=self.creatures))
            try:
                folder = staging / "fighter"
                photos = folder / "photos"
                raw = folder / "sources"
                photos.mkdir(parents=True)
                raw.mkdir()
                for key, data in images.items(): (photos / f"{key}.png").write_bytes(data)
                for key, (mime, data) in sources.items():
                    (raw / f"{key}.{'jpg' if mime == 'image/jpeg' else 'png'}").write_bytes(data)
                for key, (mime, data) in videos.items():
                    (raw / f"{key}.{'webm' if mime == 'video/webm' else 'mp4' if mime == 'video/mp4' else 'mov'}").write_bytes(data)
                (folder / "fighter.txt").write_text(creature_text(name))
                (folder / "capture.json").write_text(json.dumps({"version": 1, "name": name, "mode": mode,
                    "poses": IDS, "alignment": adjustments}, ensure_ascii=False, indent=2))
                result = subprocess.run([str(self.executable), "-data", str(self.data), "-creatures", str(staging),
                    "-sheet", name, "-shots", str(folder / "preview-"), "-nosound"],
                    cwd=ROOT, capture_output=True, text=True, timeout=90)
                if result.returncode:
                    raise ValueError("Игра не смогла собрать бойца. Проверьте кадры и попробуйте снова")
                destination = self.creatures / f"custom-{name.lower()}-{uuid.uuid4().hex[:8]}"
                os.rename(folder, destination)
                return {"name": name, "folder": destination.name, "message": "Боец готов! Вернитесь в игру и нажмите Enter."}
            finally:
                shutil.rmtree(staging, ignore_errors=True)


def handler(store, token):
    class Handler(BaseHTTPRequestHandler):
        def reply(self, status, content, mime="application/json; charset=utf-8"):
            if not isinstance(content, bytes): content = json.dumps(content, ensure_ascii=False).encode()
            self.send_response(status)
            self.send_header("Content-Type", mime)
            self.send_header("Content-Length", str(len(content)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Referrer-Policy", "no-referrer")
            self.end_headers()
            self.wfile.write(content)

        def authorized(self):
            query = parse_qs(urlsplit(self.path).query)
            supplied = self.headers.get("X-Capture-Token", query.get("token", [""])[0])
            return secrets.compare_digest(supplied, token)

        def do_GET(self):
            path = urlsplit(self.path).path
            if path == "/api/poses":
                if not self.authorized(): return self.reply(403, {"error": "Откройте ссылку из игры"})
                return self.reply(200, {"poses": [dict(zip(("id", "title", "hint"), p)) for p in POSES]})
            routes = {"/": ("index.html", "text/html; charset=utf-8"), "/app.js": ("app.js", "text/javascript; charset=utf-8"),
                      "/style.css": ("style.css", "text/css; charset=utf-8"), "/processing.js": ("processing.mjs", "text/javascript; charset=utf-8")}
            if path not in routes: return self.reply(404, {"error": "Не найдено"})
            file, mime = routes[path]
            self.reply(200, (STATIC / file).read_bytes(), mime)

        def do_POST(self):
            if urlsplit(self.path).path != "/api/fighters": return self.reply(404, {"error": "Не найдено"})
            if not self.authorized(): return self.reply(403, {"error": "Откройте ссылку из игры"})
            try:
                length = int(self.headers.get("Content-Length", "0"))
                if not 0 < length <= MAX_REQUEST: return self.reply(413, {"error": "Слишком много данных"})
                request = json.loads(self.rfile.read(length))
                if not isinstance(request, dict): raise ValueError("Некорректный запрос")
                self.reply(201, store.publish(request))
            except (ValueError, TypeError, KeyError, zlib.error) as error:
                self.reply(400, {"error": str(error)})
            except subprocess.TimeoutExpired:
                self.reply(422, {"error": "Обработка заняла слишком много времени. Попробуйте ещё раз"})
            except OSError:
                self.reply(500, {"error": "Не удалось сохранить бойца. Проверьте место на диске и сборку игры"})

        def log_message(self, fmt, *args):
            # Do not put pairing tokens from request URLs in logs.
            pass
    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--creatures", default=str(ROOT / "creatures"))
    parser.add_argument("--data", default=str(ROOT / "primalrage"))
    parser.add_argument("--ready", type=Path)
    parser.add_argument("--cert")
    parser.add_argument("--key")
    args = parser.parse_args()
    token = secrets.token_urlsafe(24)
    server = ThreadingHTTPServer((args.host, args.port), handler(CaptureStore(args.creatures, args.data, ROOT / "clayfight"), token))
    scheme = "http"
    if args.cert:
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(args.cert, args.key)
        server.socket = context.wrap_socket(server.socket, server_side=True)
        scheme = "https"
    port = server.server_port
    local = f"{scheme}://localhost:{port}/?token={token}"
    try:
        addresses = socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET)
        ip = next(a[4][0] for a in addresses if not a[4][0].startswith("127."))
    except (OSError, StopIteration):
        ip = socket.gethostname()
    phone = f"{scheme}://{ip}:{port}/?token={token}"
    if args.ready:
        args.ready.parent.mkdir(parents=True, exist_ok=True)
        temporary = args.ready.with_suffix(".tmp")
        temporary.write_text(local + "\n" + phone + "\n")
        temporary.chmod(0o600)
        temporary.replace(args.ready)
    print(f"Computer: {local}\nPhone (same Wi-Fi): {phone}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        if args.ready: args.ready.unlink(missing_ok=True)

if __name__ == "__main__": main()
