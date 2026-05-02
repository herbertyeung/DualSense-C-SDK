using System;
using System.Runtime.InteropServices;

internal static class DualSenseNative
{
    [DllImport("dualsense", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ds5_init(out IntPtr context);

    [DllImport("dualsense", CallingConvention = CallingConvention.Cdecl)]
    public static extern void ds5_shutdown(IntPtr context);
}

public static class Program
{
    public static int Main()
    {
        IntPtr context;
        int result = DualSenseNative.ds5_init(out context);
        Console.WriteLine($"ds5_init: {result}");
        if (context != IntPtr.Zero)
        {
            DualSenseNative.ds5_shutdown(context);
        }
        return result == 0 ? 0 : 1;
    }
}
