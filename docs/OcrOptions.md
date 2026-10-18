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
