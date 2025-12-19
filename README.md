# Image Resizer

## Quick Start

Run the latest version directly from the GitHub Container Registry:

```bash
docker run -d -p 8080:8080 ghcr.io/dvando/image-resizer:latest
```

## Local Development
```bash
git clone https://github.com/dvando/image-resizer
cd image-resizer
docker build -t dvando/image-resizer:latest .
docker run -d -p 8080:8080 dvando/image-resizer:latest
```

## API Documentation
**URL:** `/resize_image`  
**Method:** `POST`  
**Content-Type:** `application/json`

### Request Body

| Field | Type | Description |
| :--- | :--- | :--- |
| `input_jpeg` | `string` | The Base64 encoded string of the source JPEG. |
| `desired_width` | `integer` | The target width in pixels. |
| `desired_height` | `integer` | The target height in pixels. |

### Example Request

```json
{
  "input_jpeg": "/9j/4AAQSkZJRgABAQAAAQABAAD...",
  "desired_width": 128,
  "desired_height": 128
}
```

### Curl Example
```bash
curl -X POST http://localhost:8080/resize_image \
  -H "Content-Type: application/json" \
  -d '{
    "input_jpeg": "BASE64_STRING_HERE",
    "desired_width": 128,
    "desired_height": 128
  }'
```

### Response Code
| Status Code | Description |
| :--- | :--- |
| `200` | `Image processed successfully. Returns the resized image in Base64 encoded string.` |
| `400` | `Invalid JSON or malformed Base64 string.` |
| `500` | `Processing error on the server.` |