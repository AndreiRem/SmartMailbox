from fastapi import FastAPI, File, UploadFile, Form
from pathlib import Path
import time

app = FastAPI()

UPLOAD_DIR = Path("uploads")
UPLOAD_DIR.mkdir(exist_ok=True)


@app.post("/upload")
async def upload_photo(
    file: UploadFile = File(...),
    device_id: str = Form(...),
):
    timestamp = int(time.time())
    file_name = f"{timestamp}.jpg"

    device_upload_dir = UPLOAD_DIR.joinpath(device_id)
    device_upload_dir.mkdir(exist_ok=True)
    file_path = device_upload_dir / file_name

    with open(file_path, "wb") as f:
        f.write(await file.read())

    return {"status": "success", "filename": file_name}
