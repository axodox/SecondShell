# Finding text on screen: options (research, 2026-10-18)

What `Capture.WaitForText` should use to find text on the session's screen: good, quick, and light on CPU and memory, as the driver polls while the user works on the same machine. Findings.md 5 has the failures that started this.

## Recommendation

Keep **Windows.Media.Ocr**, and feed it better; add **UI Automation** text alongside it. Both are in Windows: nothing to ship, nothing loaded while idle, unpackaged on x64 and ARM64.

1. **Quick wins (Windows OCR):**
   - Recognize a region (the target window, or changed areas) instead of the whole screen.
   - Turn light-on-dark text dark-on-light, and upscale the region 2–3x within `MaxImageDimension`.
   - Rebuild the reading order from the words' boxes: rows by vertical overlap, left to right.
   - Match fuzzily, and skip frames that did not change.
2. **UI Automation first:** TextPattern for consoles (conhost and Windows Terminal expose their text) and editors, cached Name / Value elsewhere, scoped to the target window. OCR is the fallback for what UIA does not expose.
3. **Optional, later:** PP-OCR mobile models on ONNX Runtime (CPU), loaded only when Windows OCR fails on a region and freed when idle; measure its memory first.
4. **Optional, MSIX on Copilot+ PCs:** the Windows AI `TextRecognizer`.

## Decided design for text waits (2026-10-18)

Two tiers, so the expensive work is rare and never repeated for areas that did not change:

1. **At the start, a quick search:** UI Automation text of the windows, and Windows OCR on each window's crop (lines rebuilt from the word boxes, fuzzy matching).
2. **Not found: a thorough pass** over the whole screen (the text may already be there): overlapping bands, upscaled 1.5x, and whatever the quick search cannot see (areas without window text). While it runs, the dirty rectangles the capture reports are collected, not dropped.
3. **Then the loop, on the collected dirty rectangles:** every 250 ms the quick search over what changed since the last check; every 1–2 s, only if still not found and something changed, the thorough pass over the changed areas. A static screen costs nothing.
4. **A screen text cache** of what each area read, kept between waits, so a wait for text already on an unchanged screen answers at once.
5. **Every recognizer behind one interface** (an image region in, words with their boxes out), used by both tiers, so engines can be swapped or mixed per tier: Windows OCR first; PP-OCRv6 tiny (or its detector feeding another recognizer) on ONNX Runtime later if the quick tier misses too often in use. UI Automation sits behind a matching text source interface (a window in, words with boxes out), so the wait logic sees only words, whatever produced them. The thorough tier runs on a low-priority thread, on one core.

## What PowerToys Text Extractor does

PowerToys' screen OCR (based on Joe Finney's Text Grab; `src/modules/PowerOCR/PowerOCR.Core` in microsoft/PowerToys) uses the same engine, `Windows.Media.Ocr`, with the OCR language the user picks (it recognizes only installed OCR language packs):

- It recognizes only the region the user selects, not the screen.
- It scales the region 1.5x with high-quality bicubic interpolation when that stays within `OcrEngine::MaxImageDimension`, else 1x, else shrinks it to fit (not in word mode).
- It pads images smaller than 64 px a side (an 8 px margin, filled with the corner pixel's colour).
- No grayscale or inversion. It takes `OcrLine.Text` in the engine's order and keeps the word boxes (for clicking on a word); formatters handle single-line, table and language-specific joining.

So its robustness comes from tight crops and a modest upscale, which is what step 1 below does for waits.

## Measured (2026-10-18, this machine: Ryzen 9 9950X3D, CPU only)

Six captures from the child session (in `F:\AI\ocr-test`, with the scripts): a console with a banner only, full screen over a busy photo wallpaper and as its window; a console with more text, over the wallpaper, as its window and maximized; an Explorer window. Phrases with known text were looked for; "fuzzy" allows an edit distance of 20 % of the phrase after normalizing, with lines rebuilt from the word boxes.

| Engine | Banner over wallpaper | Banner window | Console over wallpaper | Console window | Console maximized | Explorer | Time per full screen | Time per window | Memory |
|---|---|---|---|---|---|---|---|---|---|
| Windows OCR 1x, exact | 0/3 | 0/3 | 6/9 | 6/9 | 7/9 | 11/11 | 60–70 ms | 11–18 ms | in Windows |
| Windows OCR 1x, fuzzy | 0/3 | 3/3 | 9/9 | 9/9 | 9/9 | 11/11 | | | |
| Windows OCR 1.5x, fuzzy | 0/3 | 2/3 | 1/9 | 9/9 | 9/9 | 11/11 | 70–150 ms | 11–24 ms | |
| PP-OCRv6 tiny (6.5 MB) | 3/3 | 3/3 | 9/9 | 9/9 | 9/9 | 11/11 | 1.1–1.8 s | 0.4–0.9 s | 372 MB (peak 784) |
| PP-OCRv6 small (31 MB) | 3/3 | 3/3 | 9/9 | 9/9 | 9/9 | 11/11 | 2.7–4.1 s | 1.2–2.8 s | 506 MB (peak 1069) |
| PP-OCRv6 medium (134 MB) | 3/3 | 3/3 | 9/9 | 9/9 | 9/9 | 11/11 | 9.7–14.5 s | 4–9 s | 569 MB (peak 1987) |
| PP-OCRv5 mobile | 3/3 | 2/3 | 9/9 | 9/9 | 9/9 | 11/11 | 4.4–10.3 s | 1.7–7.9 s | 467 MB (peak 1053) |

- PP-OCR ran in PaddlePaddle 3.3.1 with oneDNN off (it fails with it on, on Windows), so its times are pessimistic; ONNX Runtime, as a C++ build would use, is likely faster (not measured). Its memory is mostly the Paddle runtime.
- Windows OCR is erratic on full screens: over the wallpaper it read nothing of a console with only its banner (also at 1.5x and 2x, and not because of the capture: the same frame fails the same way from PowerShell), yet it read a console with more text. On window crops, with rebuilt lines and fuzzy matching, it found everything, in 11–24 ms. Padding the crop did not bring back a first letter it drops at the window's left edge; fuzzy matching covers that.
- PP-OCRv6 tiny read everything, including the case Windows OCR cannot, at 30–60x the time.
- **Detector plus Windows OCR** (PP-OCRv6 tiny's detection model, 2 MB, finds the text lines; Windows OCR reads each line, padded and scaled 2x): banner over wallpaper 2/3 (Windows OCR alone 0/3), consoles 9/9, Explorer 9/11 (very short boxes such as "6 items" come back empty). Detection costs ~750 ms per full screen at full resolution (~200 ms when the module scales the screen down to 960 px, which loses the console text) and ~200 ms per window, in Paddle without oneDNN; recognition of all boxes 10–60 ms.
- **Windows OCR on the window's crop alone** read the banner window 3/3 with fuzzy matching, in 11 ms: cropping to windows solves the hard case without any model. A detector earns its place only for text outside window structure (canvases, games, images), and then on regions, not the whole screen.
- Not tried: a small vision LLM (llama.cpp with SmolVLM, Qwen2.5-VL, Gemma 3): 0.5–3 GB of weights, seconds per screen on a CPU, text it may paraphrase, no reliable positions; too heavy to poll, and the AI client already reads captures itself.

## Engines

| Option | Screen text | Speed | Size | License | Requirements | Effort |
|---|---|---|---|---|---|---|
| Windows.Media.Ocr | Fair; weak on small antialiased text over busy backgrounds, line order unreliable | not published; scales with pixels, so crops help | in Windows (needs the OCR language) | OS | Windows 10+, x64 / ARM64, no package identity | in use |
| Windows AI TextRecognizer | Better, per Microsoft (no independent benchmark) | NPU | managed by Windows | Windows App SDK | Copilot+ PC (NPU), Windows 11 25H2+, package identity and `systemAIModels`, Windows App SDK 1.8+ | moderate |
| PP-OCRv5 mobile on ONNX Runtime (as RapidOCR) | Best of the light ones (Paddle's benchmark: detection 79.0, recognition 81.3 %) | Paddle, on a server Xeon: detection ~58 ms, recognition ~21 ms per crop | ~21 MB models plus ONNX Runtime | Apache-2.0, MIT | any Windows 10/11, unpackaged, ARM64 | high: pre- and post-processing, CTC decoding |
| PP-OCRv6 tiny / small (June 2026) | vendor figures only | vendor: faster than v5 | small | unverified | as above | as above; too new |
| Tesseract 5 | poor on small UI text without heavy preprocessing | fast on clean documents | ~4 MB model plus libraries | Apache-2.0 | any | moderate |
| docTR, EasyOCR, Florence-2, small VLMs | good | far too heavy to poll | 100 MB to GBs | varies | Python / PyTorch | not realistic |

## Getting more out of Windows OCR

- **Crop:** fewer pixels, less CPU, fewer false words from the wallpaper. `Direct3D11CaptureFrame` reports dirty regions on newer Windows 11, so unchanged frames can be skipped.
- **Upscale** small text 2–3x on the crop (a full 1080p frame would exceed `MaxImageDimension`).
- **Preprocess:** grayscale, stretch the contrast, invert when the region is mostly dark (consoles).
- **Reading order:** ignore `OcrResult.Lines`; group `OcrWord.BoundingRect`s into rows by vertical centre (within about half the median word height), sort by x, split columns at large gaps. Keep the word boxes for coordinates.

## Matching, whatever the engine

- Normalize both sides (NFKC, case, spacing, look-alikes such as 0/O, 1/l/I, rn/m).
- Search each line, and across line breaks, so wrapped phrases match.
- Allow an edit distance of about 15–20 % of the phrase (a bounded substring Levenshtein), and require two polls in a row against flicker. These thresholds are judgement, not from a source.

## UI Automation alongside OCR

- **Reliable text:** Win32 standard controls (Name, Value), WPF / WinUI / UWP, Chromium and Electron (native UIA by default since Chrome 138; TextPattern coverage of web content unverified), conhost and Windows Terminal (one UIA provider for the text area, with text-changed events).
- **Cost:** cross-process COM calls; use `CacheRequest` with `FindAll` to fetch many properties at once, scope to the target window, run on a background MTA thread with `IUIAutomation2` timeouts, or subscribe to text-changed events instead of polling.
- **Pitfalls:** no text from custom-drawn, DirectX, canvas and some Qt / Java interfaces; virtualized lists expose only realized items; elevated windows are out of reach (UIPI), so fall back to OCR; Name can differ from what is drawn.
- **Combination:** check UIA first, then OCR the window's region; merge both into the candidates to match, keeping element bounds or word boxes for coordinates.

## Sources

- https://learn.microsoft.com/uwp/api/windows.media.ocr.ocrengine
- https://github.com/Descolada/OCR
- https://docs.uipath.com/activities/other/latest/ui-automation/microsoft-ocr.md
- https://learn.microsoft.com/en-us/windows/ai/apis/text-recognition
- https://learn.microsoft.com/en-us/windows/ai/apis/troubleshooting
- https://www.paddleocr.ai/latest/en/version3.x/pipeline_usage/OCR.html
- https://www.paddleocr.ai/latest/en/version3.x/model_list.html
- https://aiweekly.co/alerts/paddlepaddles-pp-ocrv6-beats-billion-scale-vlms-at-ocr
- https://github.com/RapidAI/RapidOCR
- https://huggingface.co/SWHL/RapidOCR/tree/main/PP-OCRv4
- https://learn.microsoft.com/en-us/windows/ai/new-windows-ml/distributing-your-app
- https://tesseract-ocr.github.io/tessdoc/ImproveQuality.html
- https://ironsoftware.com/ocr/csharp/blog/ocr-tools/windows-ocr-engine-vs-tesseract/ (a single author's test)
- https://learn.microsoft.com/uwp/api/windows.graphics.capture
- https://developer.chrome.com/blog/windows-uia-support-update
- microsoft/terminal, `doc/terminal-a11y-2023.md`
- https://learn.microsoft.com/en-us/windows/desktop/WinAuto/uiauto-cachingforclients
- https://learn.microsoft.com/en-ie/Windows/Win32/winauto/uiauto-securityoverview

Not verified: Windows OCR's latency and minimum text size; ONNX Runtime's size and PP-OCR's memory; PP-OCRv6's license and ONNX models; Chromium's TextPattern coverage and the cost of a UIA client to Chromium; Paddle's latencies are from a server CPU.
