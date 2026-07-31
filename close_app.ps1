Add-Type @'
using System;
using System.Runtime.InteropServices;
public class Win32 {
    [DllImport("user32.dll")]
    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hWnd, uint Msg, int wParam, int lParam);
}
'@
$hwnd = [Win32]::FindWindow($null, "TouchMe3")
if ($hwnd -ne [IntPtr]::Zero) {
    [Win32]::PostMessage($hwnd, 0x0010, 0, 0) # WM_CLOSE
    Write-Host "Sent WM_CLOSE to TouchMe3"
} else {
    Write-Host "Could not find TouchMe3 window"
}

