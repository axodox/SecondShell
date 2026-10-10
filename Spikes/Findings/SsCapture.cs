using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

// Test "driver": runs inside the child session. Started either by COM (-Embedding) or directly.
// Captures the screen every 2 s, checks that its own counter window shows up fresh in the capture,
// and checks that SendInput moves the cursor.

[ComVisible(true), Guid("7B3F2C1A-5E8D-4F6B-9A2C-1D4E6F8A0B3C"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IDriverTest { void Ping(); }

[ComVisible(true), Guid("C5A1E7D2-3B9F-4E6A-8D1C-2F7B9E4A6C0D"), ClassInterface(ClassInterfaceType.None)]
public class DriverTest : IDriverTest { public void Ping() { } }

static class SsCapture
{
    [DllImport("user32.dll")] static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] static extern int GetSystemMetrics(int index);
    [DllImport("user32.dll")] static extern bool GetCursorPos(out POINT p);
    [DllImport("user32.dll", SetLastError = true)] static extern uint SendInput(uint n, INPUT[] inputs, int size);
    struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] struct MOUSEINPUT { public int dx, dy; public uint mouseData, dwFlags, time; public IntPtr extra; }
    [StructLayout(LayoutKind.Sequential)] struct INPUT { public uint type; public MOUSEINPUT mi; }

    const string OutDir = @"C:\Users\Axodox\AppData\Local\Temp\claude\h--work-Windows-Simulator-16-0\b5eeca30-5c5e-4ed2-aa32-a3a09ea9bf1b\scratchpad\sscapture\out";
    static string LogPath { get { return Path.Combine(OutDir, "loop.log"); } }
    static void Log(string s) { File.AppendAllText(LogPath, DateTime.Now.ToString("HH:mm:ss.fff") + " " + s + Environment.NewLine); }

    static Form counterForm; static Label counterLabel;

    static string MoveMouse(int i, int w, int h)
    {
        int x = 100 + (i % 10) * 50, y = 200;
        var inp = new INPUT { type = 0, mi = new MOUSEINPUT { dx = x * 65535 / (w - 1), dy = y * 65535 / (h - 1), dwFlags = 0x8001 /* MOVE|ABSOLUTE */ } };
        uint sent = SendInput(1, new[] { inp }, Marshal.SizeOf(typeof(INPUT)));
        int err = Marshal.GetLastWin32Error();
        Thread.Sleep(100);
        POINT p; GetCursorPos(out p);
        bool ok = Math.Abs(p.X - x) <= 1 && Math.Abs(p.Y - y) <= 1;
        return string.Format("sendInput={0}{1} target=({2},{3}) cursor=({4},{5}) {6}", sent, sent == 0 ? " err=" + err : "", x, y, p.X, p.Y, ok ? "OK" : "MISMATCH");
    }

    [MTAThread]
    static void Main(string[] args)
    {
        Directory.CreateDirectory(OutDir);
        // Session guard: only run in the session the host expects; otherwise exit without touching input or screen
        int mySession = Process.GetCurrentProcess().SessionId;
        string expectFile = Path.Combine(OutDir, "expected_session.txt");
        int expected;
        if (!File.Exists(expectFile) || !int.TryParse(File.ReadAllText(expectFile).Trim(), out expected) || expected != mySession)
        {
            Log("GUARD: refusing to run in session " + mySession + " (expected file " + (File.Exists(expectFile) ? File.ReadAllText(expectFile).Trim() : "missing") + ")");
            return;
        }
        bool viaCom = Array.Exists(args, a => a.TrimStart('-', '/').Equals("Embedding", StringComparison.OrdinalIgnoreCase));
        Log(string.Format("START session={0} viaCom={1} args='{2}' sizeof(INPUT)={3}", Process.GetCurrentProcess().SessionId, viaCom, string.Join(" ", args), Marshal.SizeOf(typeof(INPUT))));

        int cookie = 0;
        var rs = new RegistrationServices();
        if (viaCom) cookie = rs.RegisterTypeForComClients(typeof(DriverTest), RegistrationClassContext.LocalServer, RegistrationConnectionType.MultipleUse);

        SetProcessDPIAware();
        var ready = new ManualResetEvent(false);
        var ui = new Thread(() =>
        {
            counterForm = new Form { FormBorderStyle = FormBorderStyle.None, TopMost = true, StartPosition = FormStartPosition.Manual, Location = new Point(600, 500), Size = new Size(220, 90), BackColor = Color.White, ShowInTaskbar = false };
            counterLabel = new Label { Dock = DockStyle.Fill, Font = new Font("Segoe UI", 36, FontStyle.Bold), TextAlign = ContentAlignment.MiddleCenter, Text = "-" };
            counterForm.Controls.Add(counterLabel);
            counterForm.Shown += (s, e) => ready.Set();
            Application.Run(counterForm);
        });
        ui.SetApartmentState(ApartmentState.STA);
        ui.IsBackground = true;
        ui.Start();
        ready.WaitOne(5000);

        int lastCounterHash = 0;
        for (int i = 0; i < 25; i++)
        {
            string line;
            try
            {
                counterForm.Invoke((Action)(() => { counterLabel.Text = i.ToString(); counterForm.Refresh(); }));
                Thread.Sleep(200);
                int w = GetSystemMetrics(0), h = GetSystemMetrics(1);
                string input = MoveMouse(i, w, h);
                using (var bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb))
                {
                    using (var g = Graphics.FromImage(bmp)) g.CopyFromScreen(0, 0, 0, 0, new Size(w, h));
                    int hash = 17;
                    for (int y = 500; y < 590; y += 3)
                        for (int x = 600; x < 820; x += 3)
                            hash = hash * 31 + bmp.GetPixel(x, y).ToArgb();
                    bool fresh = hash != lastCounterHash;
                    lastCounterHash = hash;
                    bmp.Save(Path.Combine(OutDir, "loop_" + i.ToString("00") + ".png"), ImageFormat.Png);
                    line = string.Format("#{0} capture {1}x{2} counterRegion={3} | {4}", i, w, h, fresh ? "FRESH" : "STALE", input);
                }
            }
            catch (Exception ex) { line = "#" + i + " ERROR " + ex.GetType().Name + ": " + ex.Message; }
            Log(line);
            Thread.Sleep(1800);
        }

        if (viaCom) rs.UnregisterTypeForComClients(cookie);
        Log("END");
    }
}
