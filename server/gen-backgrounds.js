const fs = require('fs');
const path = require('path');
const sharp = require('sharp');
const dir = path.resolve(__dirname, 'backgrounds');

const API_URL = 'https://api.tokentrove.co/v1beta/models/gemini-3.1-flash-image-preview:generateContent';
const API_KEY = 'REDACTED_API_KEY';

const STYLE_PROMPT = `Technical requirements:
- Exact resolution: 480x266 pixels (widescreen landscape)
- Target display: RGB565 (16-bit, 5-6-5 per channel, only 65K colors)
- AVOID smooth gradients — they will show ugly color banding on this display
- Use flat cel-shaded coloring with solid color blocks and bold outlines
- Keep the palette limited: prefer distinct, separated color values over subtle transitions

Art style requirements:
- Japanese anime background art style (like Makoto Shinkai or Ghibli backgrounds)
- Calm, romantic, warm mood
- Simple composition with few elements — avoid clutter
- Dark enough to serve as background behind white text, but still beautiful
- Warm color palette preferred (amber, warm orange, soft pink, deep navy)`;

async function generateBg(name, prompt) {
  console.log(`[${name}] Generating...`);

  const fullPrompt = `${STYLE_PROMPT}\n\nScene: ${prompt}`;

  const resp = await fetch(`${API_URL}?key=${API_KEY}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      contents: [{ parts: [{ text: fullPrompt }] }],
      generationConfig: {
        responseModalities: ['image', 'text'],
      }
    })
  });

  if (!resp.ok) {
    const err = await resp.text();
    console.error(`[${name}] API error ${resp.status}: ${err}`);
    return false;
  }

  const json = await resp.json();
  const parts = json.candidates?.[0]?.content?.parts || [];
  const imgPart = parts.find(p => p.inlineData);

  if (!imgPart) {
    console.error(`[${name}] No image in response`);
    return false;
  }

  const buf = Buffer.from(imgPart.inlineData.data, 'base64');
  const outPath = path.join(dir, `${name}.jpg`);

  // Resize to exact dimensions and compress
  await sharp(buf)
    .resize(480, 266, { fit: 'cover' })
    .jpeg({ quality: 75 })
    .toFile(outPath);

  const size = fs.statSync(outPath).size;
  console.log(`[${name}] Saved! (${(size/1024).toFixed(1)}KB)`);
  return true;
}

async function main() {
  if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });

  const tasks = [
    ['weather_clear', 'Warm sunset over a Japanese coastal town, a girl silhouette standing on a hill looking at the amber sky, few simple clouds with golden edges, power lines in the distance. Warm orange and deep navy palette.'],
    ['weather_cloud', 'Overcast afternoon over Japanese countryside rooftops, a girl sitting by a window reading, soft gray-blue clouds, warm indoor lamplight visible. Muted warm tones.'],
    ['weather_rain', 'Quiet rainy evening on a Japanese shopping street, a girl with a red umbrella walking alone, warm street lanterns reflecting on wet pavement, soft amber and deep blue.'],
    ['weather_snow', 'Peaceful snowy night in a small Japanese village, warm lantern light on fresh snow, a girl in a scarf walking on a quiet path, deep indigo sky with a few stars.'],
    ['weather_storm', 'Dramatic anime storm sky over a Japanese seaside, dark clouds with breaks of warm amber light, wind-blown trees, a lighthouse in the distance. Dark purple-gray and warm amber.'],
    ['stocks', 'Quiet Japanese city rooftop at twilight, a girl leaning on a railing looking at the warm city lights below, simple skyline silhouette, warm amber and soft navy sky.'],
    ['news', 'Anime girl sitting on a grassy hillside at night, looking up at a large moon and scattered stars, a quiet Japanese town with warm lights in the valley below. Soft warm palette with deep navy sky.'],
  ];

  for (const [name, prompt] of tasks) {
    await generateBg(name, prompt);
    await new Promise(r => setTimeout(r, 2000));
  }

  console.log('\nAll backgrounds generated!');
}

main();
