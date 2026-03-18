const express = require('express');
const RSSParser = require('rss-parser');
const path = require('path');
const fs = require('fs');
const app = express();
const PORT = 3100;
const rss = new RSSParser();
app.use(express.json());

// ============ Cache ============
let cache = {
  weather: null,
  stocks: null,
  news: null,
  weatherTime: 0,
  stocksTime: 0,
  newsTime: 0,
};

const WEATHER_TTL = 10 * 60 * 1000;  // 10 min
const STOCKS_TTL  = 60 * 1000;       // 1 min
const NEWS_TTL    = 30 * 60 * 1000;  // 30 min

// ============ Weather (Open-Meteo, no API key) ============
// Niagara Falls, ON: lat 43.09, lon -79.08
async function fetchWeather() {
  if (cache.weather && Date.now() - cache.weatherTime < WEATHER_TTL) {
    return cache.weather;
  }

  try {
    const url = 'https://api.open-meteo.com/v1/forecast?latitude=43.09&longitude=-79.08' +
      '&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m' +
      '&daily=temperature_2m_max,temperature_2m_min&timezone=America/Toronto&forecast_days=1';

    const res = await fetch(url);
    const data = await res.json();
    const cur = data.current;

    // Map WMO weather codes to simple icon names
    const code = cur.weather_code;
    let icon = 'cloud', description = 'Cloudy';
    if (code === 0) { icon = 'sun'; description = 'Clear Sky'; }
    else if (code <= 3) { icon = 'cloud'; description = ['Mainly Clear', 'Partly Cloudy', 'Overcast'][code - 1]; }
    else if (code <= 48) { icon = 'fog'; description = 'Foggy'; }
    else if (code <= 57) { icon = 'rain'; description = 'Drizzle'; }
    else if (code <= 65) { icon = 'rain'; description = 'Rain'; }
    else if (code <= 67) { icon = 'rain'; description = 'Freezing Rain'; }
    else if (code <= 75) { icon = 'snow'; description = 'Snow'; }
    else if (code === 77) { icon = 'snow'; description = 'Snow Grains'; }
    else if (code <= 82) { icon = 'rain'; description = 'Rain Showers'; }
    else if (code <= 86) { icon = 'snow'; description = 'Snow Showers'; }
    else if (code <= 99) { icon = 'storm'; description = 'Thunderstorm'; }

    cache.weather = {
      city: "Niagara Falls",
      temp: Math.round(cur.temperature_2m),
      feels_like: Math.round(cur.apparent_temperature),
      humidity: Math.round(cur.relative_humidity_2m),
      description,
      icon,
      wind: Math.round(cur.wind_speed_10m),
      high: Math.round(data.daily.temperature_2m_max[0]),
      low: Math.round(data.daily.temperature_2m_min[0]),
    };
    cache.weatherTime = Date.now();
    console.log('[Weather] Updated:', cache.weather.temp + '°C', cache.weather.description);
  } catch (e) {
    console.error('[Weather] Error:', e.message);
    if (!cache.weather) {
      cache.weather = { city: "Toronto", temp: 0, feels_like: 0, humidity: 0, description: "N/A", icon: "cloud", wind: 0, high: 0, low: 0 };
    }
  }
  return cache.weather;
}

// ============ Stocks (Yahoo Finance, no API key) ============
const STOCK_SYMBOLS = ['AAPL', 'NVDA', 'AMD', '^IXIC', '^GSPC', 'TSLA'];
// Display names for index symbols
const DISPLAY_NAMES = { '^IXIC': 'NASDAQ', '^GSPC': 'S&P500' };

async function fetchStocks() {
  if (cache.stocks && Date.now() - cache.stocksTime < STOCKS_TTL) {
    return cache.stocks;
  }

  try {
    const results = await Promise.all(STOCK_SYMBOLS.map(async (symbol) => {
      const url = `https://query1.finance.yahoo.com/v8/finance/chart/${symbol}?interval=1d&range=1d`;
      const res = await fetch(url, { headers: { 'User-Agent': 'Mozilla/5.0' } });
      const data = await res.json();
      const meta = data.chart.result[0].meta;
      const price = meta.regularMarketPrice;
      const prevClose = meta.chartPreviousClose;
      const change = price - prevClose;
      const pct = (change / prevClose) * 100;
      return {
        symbol: DISPLAY_NAMES[symbol] || symbol,
        price: parseFloat(price.toFixed(2)),
        change: parseFloat(change.toFixed(2)),
        pct: parseFloat(pct.toFixed(2)),
      };
    }));

    cache.stocks = results;
    cache.stocksTime = Date.now();
    console.log('[Stocks] Updated:', cache.stocks.length, 'symbols');
  } catch (e) {
    console.error('[Stocks] Error:', e.message);
    if (!cache.stocks) {
      cache.stocks = STOCK_SYMBOLS.map(s => ({ symbol: s, price: 0, change: 0, pct: 0 }));
    }
  }
  return cache.stocks;
}

// ============ News (Google News RSS + AI summary) ============
const GEMINI_URL = 'https://api.tokentrove.co/v1beta/models/gemini-3.1-flash-lite-preview:generateContent';
const GEMINI_KEY = 'REDACTED_API_KEY';

async function callGemini(prompt) {
  try {
    const resp = await fetch(`${GEMINI_URL}?key=${GEMINI_KEY}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        contents: [{ parts: [{ text: prompt }] }],
        generationConfig: { maxOutputTokens: 1000, thinkingConfig: { thinkingBudget: 0 } }
      })
    });
    const json = await resp.json();
    return json.candidates?.[0]?.content?.parts?.[0]?.text?.trim() || '';
  } catch (e) {
    console.error('[Gemini] Error:', e.message);
    return '';
  }
}

async function generateSummary(title) {
  return callGemini(`Based on this news headline, write a summary of exactly 350-400 English characters (about 4-5 sentences). Explain what the story is about, why it matters, and key details. Be factual and informative. Respond with ONLY the summary, no prefix.\n\nHeadline: ${title}`);
}

async function translateToZh(title, summary) {
  const result = await callGemini(`Translate the following news title and summary to Simplified Chinese. The Chinese summary MUST be 150-200 Chinese characters (not longer, the display is small). Respond in JSON format: {"title":"...","summary":"..."}\n\nTitle: ${title}\nSummary: ${summary}`);
  try {
    const clean = result.replace(/```json\n?|\n?```/g, '').trim();
    return JSON.parse(clean);
  } catch {
    return { title: title, summary: summary };
  }
}

async function fetchNews() {
  if (cache.news && Date.now() - cache.newsTime < NEWS_TTL) {
    return cache.news;
  }

  try {
    const feed = await rss.parseURL('https://news.google.com/rss/topics/CAAqJggKIiBDQkFTRWdvSUwyMHZNRGRqTVhZU0FtVnVHZ0pWVXlnQVAB?hl=en-US&gl=US&ceid=US:en');
    const items = feed.items.slice(0, 5).map(item => ({
      title: item.title.replace(/ - .*$/, ''),
      source: item.title.match(/ - ([^-]+)$/)?.[1]?.trim() || 'News',
      summary: '',
      title_zh: '',
      summary_zh: '',
    }));

    // Generate summaries, then translate — all in parallel
    await Promise.all(items.map(async (item) => {
      item.summary = await generateSummary(item.title);
      const zh = await translateToZh(item.title, item.summary);
      item.title_zh = zh.title;
      item.summary_zh = zh.summary;
    }));

    cache.news = items;
    cache.newsTime = Date.now();
    console.log('[News] Updated:', cache.news.length, 'articles with summaries');
  } catch (e) {
    console.error('[News] Error:', e.message);
    if (!cache.news) {
      cache.news = [{ title: "Unable to load news", source: "Error", summary: "" }];
    }
  }
  return cache.news;
}

// ============ API Endpoints ============

app.get('/api/dashboard', async (req, res) => {
  const [weather, stocks, news] = await Promise.all([
    fetchWeather(), fetchStocks(), fetchNews()
  ]);
  res.json({ weather, stocks, news, timestamp: Date.now() });
});

app.get('/api/news/:id', async (req, res) => {
  const news = await fetchNews();
  const idx = parseInt(req.params.id);
  if (idx >= 0 && idx < news.length) {
    res.json(news[idx]);
  } else {
    res.status(404).json({ error: 'Not found' });
  }
});
app.get('/api/weather', async (req, res) => res.json(await fetchWeather()));
app.get('/api/stocks', async (req, res) => res.json(await fetchStocks()));
app.get('/api/news', async (req, res) => res.json(await fetchNews()));
app.get('/api/ping', (req, res) => res.json({ status: "ok" }));

// Preview UI
app.get('/preview', (req, res) => res.sendFile(path.resolve(__dirname, 'preview.html'), { dotfiles: 'allow' }));

// Background images - rotate hourly from multiple variants
const BG_DIR = path.resolve(__dirname, 'backgrounds');
const WEATHER_ICON_MAP = {
  sun: 'weather_clear', cloud: 'weather_cloud', fog: 'weather_cloud',
  rain: 'weather_rain', snow: 'weather_snow', storm: 'weather_storm',
};

// Find all variants for a given prefix (e.g. "stocks" -> ["stocks_1.jpg", "stocks_2.jpg", ...])
function getBgVariants(prefix) {
  try {
    return fs.readdirSync(BG_DIR)
      .filter(f => f.startsWith(prefix + '_') && f.endsWith('.jpg'))
      .sort();
  } catch { return []; }
}

// Pick variant based on current minute (rotates every minute)
function pickBgByMinute(prefix) {
  let variants = getBgVariants(prefix);
  if (variants.length === 0) {
    const single = prefix + '.jpg';
    if (fs.existsSync(path.join(BG_DIR, single))) return single;
    return null;
  }
  const now = new Date();
  const minuteOfDay = now.getHours() * 60 + now.getMinutes();
  return variants[minuteOfDay % variants.length];
}

app.get('/api/bg/weather', async (req, res) => {
  const w = cache.weather || await fetchWeather();
  const prefix = WEATHER_ICON_MAP[w.icon] || 'weather_cloud';
  const file = pickBgByMinute(prefix);
  if (file) res.sendFile(path.join(BG_DIR, file), { dotfiles: 'allow' });
  else res.status(404).send('No background');
});
app.get('/api/bg/stocks', (req, res) => {
  const file = pickBgByMinute('stocks');
  if (file) res.sendFile(path.join(BG_DIR, file), { dotfiles: 'allow' });
  else res.status(404).send('No background');
});
app.get('/api/bg/news', (req, res) => {
  const file = pickBgByMinute('news');
  if (file) res.sendFile(path.join(BG_DIR, file), { dotfiles: 'allow' });
  else res.status(404).send('No background');
});

// Audio files
const AUDIO_DIR = path.resolve(__dirname, 'audio');
app.get('/api/audio/:name', (req, res) => {
  res.sendFile(path.join(AUDIO_DIR, req.params.name), { dotfiles: 'allow' });
});

app.listen(PORT, '0.0.0.0', () => {
  console.log(`[ESP32 API] Running on http://0.0.0.0:${PORT}`);
  // Pre-fetch all data on startup
  Promise.all([fetchWeather(), fetchStocks(), fetchNews()])
    .then(() => console.log('[ESP32 API] All data pre-fetched'))
    .catch(e => console.error('[ESP32 API] Pre-fetch error:', e.message));
});
