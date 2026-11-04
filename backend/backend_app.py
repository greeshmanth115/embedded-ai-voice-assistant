import os
import uuid
import asyncio
from pathlib import Path

import requests
import pyttsx3
import soundfile as sf
from fastapi import FastAPI, UploadFile, File, HTTPException
from fastapi.staticfiles import StaticFiles
from faster_whisper import WhisperModel

BASE_DIR = Path(__file__).resolve().parent
UPLOAD_DIR = BASE_DIR / "uploads"
AUDIO_DIR = BASE_DIR / "generated_audio"
UPLOAD_DIR.mkdir(exist_ok=True)
AUDIO_DIR.mkdir(exist_ok=True)

OLLAMA_URL = os.getenv("OLLAMA_URL", "http://127.0.0.1:11434")
OLLAMA_MODEL = os.getenv("OLLAMA_MODEL", "qwen2.5:3b")
WHISPER_MODEL = os.getenv("WHISPER_MODEL", "base")
PUBLIC_BASE_URL = os.getenv("PUBLIC_BASE_URL", "").rstrip("/")

app = FastAPI(title="Heidi Voice Assistant API")
app.mount("/audio", StaticFiles(directory=str(AUDIO_DIR)), name="audio")

print(f"Loading Whisper model: {WHISPER_MODEL}")
whisper = WhisperModel(WHISPER_MODEL, device="cpu", compute_type="int8")


def transcribe_audio(path: Path) -> str:
    segments, _ = whisper.transcribe(str(path), beam_size=5, language="en")
    return " ".join(segment.text.strip() for segment in segments).strip()


def ask_ollama(text: str) -> str:
    payload = {
        "model": OLLAMA_MODEL,
        "messages": [
            {
                "role": "system",
                "content": (
                    "You are Heidi, a cute and friendly female voice assistant. "
                    "Speak in English. Keep answers short, clear, and natural."
                ),
            },
            {"role": "user", "content": text},
        ],
        "stream": False,
    }
    response = requests.post(
        f"{OLLAMA_URL}/api/chat",
        json=payload,
        timeout=120,
    )
    response.raise_for_status()
    return response.json()["message"]["content"].strip()


def create_speech(text: str, output_path: Path) -> None:
    engine = pyttsx3.init()
    try:
        # Prefer Microsoft's Zira voice when available on Windows.
        for voice in engine.getProperty("voices"):
            details = f"{voice.id} {voice.name}".lower()
            if "zira" in details:
                engine.setProperty("voice", voice.id)
                break
        engine.setProperty("rate", 175)
        engine.setProperty("volume", 1.0)
        engine.save_to_file(text, str(output_path))
        engine.runAndWait()
    finally:
        engine.stop()

    if not output_path.exists() or output_path.stat().st_size < 44:
        raise RuntimeError("Text-to-speech did not create a valid WAV file")


@app.get("/")
def root():
    return {
        "status": "ok",
        "assistant": "Heidi",
        "ollama_model": OLLAMA_MODEL,
        "whisper_model": WHISPER_MODEL,
    }


@app.get("/health")
def health():
    return {"status": "ok"}


@app.post("/voice")
async def voice(file: UploadFile = File(...)):
    if not file.filename.lower().endswith((".wav", ".wave")):
        raise HTTPException(status_code=400, detail="Please upload a WAV file")

    request_id = uuid.uuid4().hex
    input_path = UPLOAD_DIR / f"{request_id}.wav"
    output_path = AUDIO_DIR / f"{request_id}.wav"

    try:
        contents = await file.read()
        if not contents:
            raise HTTPException(status_code=400, detail="Empty audio file")
        input_path.write_bytes(contents)

        transcript = await asyncio.to_thread(transcribe_audio, input_path)
        if not transcript:
            raise HTTPException(status_code=422, detail="No speech detected")

        reply = await asyncio.to_thread(ask_ollama, transcript)
        await asyncio.to_thread(create_speech, reply, output_path)

        audio_url = f"/audio/{output_path.name}"
        if PUBLIC_BASE_URL:
            audio_url = f"{PUBLIC_BASE_URL}{audio_url}"

        return {
            "transcript": transcript,
            "reply": reply,
            "audio_url": audio_url,
        }
    except HTTPException:
        raise
    except requests.RequestException as exc:
        raise HTTPException(
            status_code=502,
            detail=f"Could not contact Ollama at {OLLAMA_URL}: {exc}",
        ) from exc
    except Exception as exc:
        raise HTTPException(status_code=500, detail=str(exc)) from exc
    finally:
        await file.close()
        if input_path.exists():
            input_path.unlink(missing_ok=True)
