require('dotenv').config();
const fs = require('fs');
const path = require('path');
const sharp = require('sharp');
const dir = path.resolve(__dirname, 'backgrounds');

const API_URL = 'https://api.tokentrove.co/v1beta/models/gemini-3.1-flash-image-preview:generateContent';
const API_KEY = process.env.GEMINI_API_KEY;

const STYLE_BASE = `Technical requirements:
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
  // Skip if already exists
  if (fs.existsSync(path.join(dir, `${name}.jpg`))) {
    console.log(`[${name}] Already exists, skipping`);
    return true;
  }

  console.log(`[${name}] Generating...`);
  const fullPrompt = `${STYLE_BASE}\n\nScene: ${prompt}`;

  try {
    const resp = await fetch(`${API_URL}?key=${API_KEY}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        contents: [{ parts: [{ text: fullPrompt }] }],
        generationConfig: { responseModalities: ['image', 'text'] }
      })
    });

    if (!resp.ok) {
      console.error(`[${name}] API error ${resp.status}`);
      return false;
    }

    const json = await resp.json();
    const parts = json.candidates?.[0]?.content?.parts || [];
    const imgPart = parts.find(p => p.inlineData);
    if (!imgPart) { console.error(`[${name}] No image`); return false; }

    const buf = Buffer.from(imgPart.inlineData.data, 'base64');
    await sharp(buf).resize(480, 266, { fit: 'cover' }).jpeg({ quality: 75 }).toFile(path.join(dir, `${name}.jpg`));
    const size = fs.statSync(path.join(dir, `${name}.jpg`)).size;
    console.log(`[${name}] OK (${(size/1024).toFixed(1)}KB)`);
    return true;
  } catch (e) {
    console.error(`[${name}] Error: ${e.message}`);
    return false;
  }
}

async function main() {
  if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });

  const tasks = [
    // ========== WEATHER CLEAR (6) — sky, sun, open landscapes ==========
    ['weather_clear_1', 'Warm sunset over a Japanese coastal town, girl silhouette on a hill, amber sky, power lines. Warm orange and deep navy.'],
    ['weather_clear_2', 'Golden hour at a Japanese shrine, torii gate against peach-orange sky, cherry blossoms drifting, girl on stone steps.'],
    ['weather_clear_3', 'Girl sitting on a rooftop water tank at sunset, clear sky fading orange to purple, distant mountains, Japanese town below.'],
    ['weather_clear_4', 'Japanese beach at golden hour, girl walking barefoot, calm sea reflecting amber light, seagulls, vast clear sky.'],
    ['weather_clear_5', 'Endless sunflower field under a clear blue anime sky, girl with a straw hat, distant Japanese farmhouse, warm summer afternoon.'],
    ['weather_clear_6', 'Girl lying on grass atop a Japanese hillside, clear sky with a few wispy clouds, panoramic view of green valleys, warm golden light.'],

    // ========== WEATHER CLOUD (6) — overcast, layered clouds ==========
    ['weather_cloud_1', 'Overcast afternoon over Japanese countryside rooftops, girl by a window reading, gray-blue clouds, warm lamplight. Muted tones.'],
    ['weather_cloud_2', 'Japanese train station platform on a cloudy day, girl waiting, diffused light, mountains behind gray clouds. Blue-gray.'],
    ['weather_cloud_3', 'Girl on a bicycle on a Japanese country road, overcast sky, dramatic layered clouds, rice paddies, wind turbines.'],
    ['weather_cloud_4', 'Cloudy afternoon at a Japanese river bridge, girl leaning on railing, gray-lavender sky, autumn trees with orange leaves.'],
    ['weather_cloud_5', 'Vast Japanese grassland under heavy gray clouds, girl standing with wind blowing her hair, distant mountain range, moody light.'],
    ['weather_cloud_6', 'Japanese coastal cliff on an overcast day, girl sitting on the edge looking at gray sea, dramatic cloud layers, muted palette.'],

    // ========== WEATHER RAIN (5) — rain, puddles, umbrellas ==========
    ['weather_rain_1', 'Rainy evening on a Japanese shopping street, girl with red umbrella, lanterns reflecting on wet pavement, amber and deep blue.'],
    ['weather_rain_2', 'Girl under a bus shelter in rain, vending machine glow, puddles reflecting neon, quiet Japanese suburban street. Blue and amber.'],
    ['weather_rain_3', 'Rainy afternoon at a Japanese school courtyard, girl watching rain from covered walkway, puddles, gray-blue with warm light.'],
    ['weather_rain_4', 'Japanese garden in gentle rain, girl with transparent umbrella among wet autumn maples, stone lantern, koi pond with raindrops.'],
    ['weather_rain_5', 'Heavy rain on a Japanese mountain road, girl running with a bag over her head, misty mountains, deep green and gray-blue.'],

    // ========== WEATHER SNOW (5) — winter, snow, cold ==========
    ['weather_snow_1', 'Snowy night in a small Japanese village, warm lantern light on snow, girl in scarf on quiet path, indigo sky with stars.'],
    ['weather_snow_2', 'Girl catching snowflakes at a Japanese temple, warm lamplight, snow-covered stone path, bare trees, navy sky.'],
    ['weather_snow_3', 'Snowy evening at a Japanese hot spring town, warm light from wooden buildings, steam rising, girl on bridge over stream.'],
    ['weather_snow_4', 'Quiet snowy morning in a Japanese residential street, girl with mittens, fresh untouched snow, warm light from houses, lavender sky.'],
    ['weather_snow_5', 'Frozen Japanese lake surrounded by snowy mountains, girl standing on the shore, pale winter sun, deep blue and white palette.'],

    // ========== WEATHER STORM (5) — dramatic, dark, lightning ==========
    ['weather_storm_1', 'Storm sky over Japanese seaside, dark clouds with amber light breaks, wind-blown trees, lighthouse. Purple-gray and amber.'],
    ['weather_storm_2', 'Thunderstorm over a Japanese mountain town, dark clouds with lightning glow, girl on a porch, wind-blown rain. Purple and amber.'],
    ['weather_storm_3', 'Stormy dusk at a Japanese fishing port, swirling dark clouds, boats rocking, warm lantern on dock, crashing waves.'],
    ['weather_storm_4', 'Dramatic storm clouds rolling over Japanese rice fields, girl running toward a farmhouse, wind bending tall grass, dark teal sky.'],
    ['weather_storm_5', 'Anime typhoon approaching a Japanese coastal town, massive dark clouds, palm trees bending, girl watching from a high window.'],

    // ========== STOCKS (25) — money, business, city, commerce ==========
    ['stocks_1', 'Japanese city rooftop at twilight, girl leaning on railing looking at warm city lights below, skyline silhouette, amber and navy.'],
    ['stocks_2', 'Girl at a cafe window at night, Japanese city street outside with neon reflections, coffee cup, cozy warm light. Amber and blue.'],
    ['stocks_3', 'Nighttime view from a Japanese apartment balcony, city skyline twinkling, girl gazing out, warm indoor light. Navy and amber.'],
    ['stocks_4', 'Japanese downtown at dusk, warm street lamps, girl walking past shop fronts, geometric buildings, orange to navy sky.'],
    ['stocks_5', 'Anime girl in a Japanese stock trading floor, multiple glowing screens with charts, warm amber monitors in dark room, focused atmosphere.'],
    ['stocks_6', 'Japanese convenience store at night, warm fluorescent glow, girl at the counter, coins and bills visible, quiet street outside.'],
    ['stocks_7', 'Girl sitting on stacked gold coins on a Japanese rooftop, city lights as bokeh background, warm amber and navy, dreamy mood.'],
    ['stocks_8', 'Japanese department store at evening, warm window displays, girl window shopping, price tags visible, elegant warm lighting.'],
    ['stocks_9', 'Anime girl at a Japanese bank ATM at night, screen glow on her face, marble interior, warm and cold light contrast.'],
    ['stocks_10', 'Bustling Japanese fish market at dawn, warm lantern light, vendors and stalls, girl browsing fresh produce, amber morning tones.'],
    ['stocks_11', 'Girl in a Japanese office at night, city skyline through floor-to-ceiling windows, desk with laptop, warm desk lamp, blue city outside.'],
    ['stocks_12', 'Japanese vending machine alley at night, colorful machine glow, girl choosing a drink, warm and neon colors, quiet atmosphere.'],
    ['stocks_13', 'Anime girl counting coins at a Japanese shrine offering box, warm lantern light, autumn leaves, peaceful commercial scene.'],
    ['stocks_14', 'Japanese shopping arcade (shotengai) at sunset, warm string lights, small shops, girl with shopping bags, amber tones.'],
    ['stocks_15', 'Girl sitting at a Japanese ramen stall at night, warm steam rising, chef cooking, noren curtain, cozy amber lighting.'],
    ['stocks_16', 'Japanese port with cargo ships at sunset, girl on the dock watching commerce, cranes silhouette, warm industrial amber and navy.'],
    ['stocks_17', 'Anime girl walking through a Japanese bookstore, warm interior, stacked books and magazines, soft amber light from windows.'],
    ['stocks_18', 'Japanese train with girl looking out window at passing city lights at night, reflection in glass, warm interior cold exterior.'],
    ['stocks_19', 'Rooftop Japanese garden bar at twilight, girl at a high table, city lights behind, cocktail glass, warm ambient lighting.'],
    ['stocks_20', 'Japanese tech district at night, girl walking among glowing signs and screens, futuristic but warm, neon and amber mix.'],
    ['stocks_21', 'Girl at a Japanese antique shop, warm interior, old clocks and treasures, amber lamplight, wooden shelves, nostalgic commerce.'],
    ['stocks_22', 'Japanese flower shop storefront at dusk, girl arranging bouquets, warm light spilling onto sidewalk, colorful flowers, cozy.'],
    ['stocks_23', 'Anime girl at a Japanese luxury watch counter, glass display cases, warm spot lighting, elegant dark interior.'],
    ['stocks_24', 'Japanese taxi in rain at night, girl in back seat, meter visible, city lights streaking through wet window, warm amber interior.'],
    ['stocks_25', 'Girl at a Japanese outdoor food festival, warm lanterns, stall signs with prices, crowd silhouettes, festive amber atmosphere.'],

    // ========== NEWS (25) — reading, books, newspapers, current affairs, conflict ==========
    ['news_1', 'Girl on a grassy hillside at night looking at large moon, Japanese town with warm lights in valley below. Navy and amber.'],
    ['news_2', 'Girl reading under a large tree at twilight, Japanese countryside, warm lights, large moon rising, fireflies. Navy and amber.'],
    ['news_3', 'Quiet Japanese library at night, girl at desk by large window, warm desk lamp, moonlight, bookshelves in shadow. Amber and indigo.'],
    ['news_4', 'Girl sitting on a wooden dock at a Japanese lake at dusk, warm lantern beside her, still water reflecting sunset, distant mountains.'],
    ['news_5', 'Anime girl reading a newspaper on a Japanese park bench at dawn, soft morning light, pigeons, quiet trees, warm peaceful tones.'],
    ['news_6', 'Girl in a Japanese bookshop corner, sitting on floor surrounded by stacked books, warm reading lamp, cozy amber atmosphere.'],
    ['news_7', 'Japanese newsstand at twilight, girl picking up a paper, headlines visible, warm street light, rows of magazines, amber tones.'],
    ['news_8', 'Girl sitting in a Japanese window seat reading, rain outside, warm blanket, tea cup, stack of newspapers, cozy amber interior.'],
    ['news_9', 'Anime girl at a Japanese university campus, carrying books, autumn maple trees, warm golden light, stone buildings.'],
    ['news_10', 'Japanese war memorial at sunset, girl standing before stone monument, fallen leaves, dramatic amber sky, solemn but warm mood.'],
    ['news_11', 'Girl writing in a journal at a Japanese coffee shop, newspapers on the table, warm interior, city visible through window.'],
    ['news_12', 'Anime girl broadcasting from a Japanese radio station, microphone and papers, warm studio lighting, city skyline through window at night.'],
    ['news_13', 'Japanese castle ruins at dusk, girl exploring with a book, dramatic sky, historical atmosphere, warm amber and deep purple.'],
    ['news_14', 'Girl on a Japanese rooftop at night, holding binoculars looking at city, warm city glow, deep navy sky, observer mood.'],
    ['news_15', 'Quiet Japanese study room with tatami, girl reading by candlelight, scrolls and old books, warm amber, traditional atmosphere.'],
    ['news_16', 'Anime girl at a Japanese printing press, warm light on old machines and fresh newspaper sheets, industrial amber tones.'],
    ['news_17', 'Japanese peace garden at sunset, girl sitting by a stone lantern reading, koi pond, cherry blossoms, warm golden hour light.'],
    ['news_18', 'Girl watching an old CRT TV showing news in a dark Japanese room, TV glow illuminating her face, warm and blue contrast.'],
    ['news_19', 'Japanese post office at evening, girl mailing a letter, warm interior light, stacks of parcels and mail, amber and brown tones.'],
    ['news_20', 'Anime girl sitting on a tank turret in a field of flowers, peaceful post-conflict scene, warm sunset, Japanese anime military aesthetic.'],
    ['news_21', 'Girl typing on a typewriter at a Japanese desk, stacks of paper and reference books, warm desk lamp, focused atmosphere.'],
    ['news_22', 'Japanese harbor with battleship in background at sunset, girl on dock watching, dramatic warm amber sky, historical atmosphere.'],
    ['news_23', 'Girl pinning articles on a Japanese investigation board, photos and string connections, warm lamp, detective/journalist mood.'],
    ['news_24', 'Anime girl standing before a giant globe in a Japanese museum, warm ambient light, maps on walls, explorer and knowledge theme.'],
    ['news_25', 'Japanese rooftop with satellite dishes and antennas at dawn, girl with laptop, first morning light, warm pink-orange sky, information age.'],
  ];

  let success = 0, fail = 0;
  for (const [name, prompt] of tasks) {
    const ok = await generateBg(name, prompt);
    if (ok) success++; else fail++;
    await new Promise(r => setTimeout(r, 3000)); // 3s between calls to avoid rate limits
  }

  console.log(`\nDone! ${success} generated, ${fail} failed, ${tasks.length} total`);
}

main();
