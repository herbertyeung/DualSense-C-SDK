using System;
using System.Runtime.InteropServices;

internal enum Ds5Result : int
{
    Ok = 0,
    InsufficientBuffer = -8,
    Timeout = -11,
}

internal enum Ds5Transport : int
{
    Unknown = 0,
    Usb = 1,
    Bluetooth = 2,
}

[StructLayout(LayoutKind.Sequential)]
internal struct Ds5Capabilities
{
    public uint size;
    public uint version;
    public uint flags;
    public Ds5Transport transport;
}

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
internal struct Ds5DeviceInfo
{
    public uint size;
    public uint version;
    public ushort vendor_id;
    public ushort product_id;
    public Ds5Transport transport;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
    public string path;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
    public string serial;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
    public string product;
    public Ds5Capabilities capabilities;
}

[StructLayout(LayoutKind.Sequential)]
internal struct Ds5TouchPoint
{
    public byte active;
    public byte id;
    public ushort x;
    public ushort y;
}

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct Ds5State
{
    public uint size;
    public uint version;
    public uint buttons;
    public int dpad;
    public byte left_stick_x;
    public byte left_stick_y;
    public byte right_stick_x;
    public byte right_stick_y;
    public byte left_trigger;
    public byte right_trigger;
    public short gyro_x;
    public short gyro_y;
    public short gyro_z;
    public short accel_x;
    public short accel_y;
    public short accel_z;
    public byte battery_percent;
    public fixed byte touch[12];
    public Ds5Transport transport;
    public uint raw_report_size;
    public fixed byte raw_report[128];
}

internal static class DualSenseNative
{
    [DllImport("dualsense", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ds5_init(out IntPtr context);

    [DllImport("dualsense", CallingConvention = CallingConvention.Cdecl)]
    public static extern void ds5_shutdown(IntPtr context);

    [DllImport("dualsense", CallingConvention = CallingConvention.Cdecl)]
    public static extern void ds5_device_info_init(ref Ds5DeviceInfo info);

    [DllImport("dualsense", CallingConvention = CallingConvention.Cdecl)]
    public static extern void ds5_state_init(ref Ds5State state);

    [DllImport("dualsense", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ds5_enumerate(IntPtr context, [Out] Ds5DeviceInfo[]? devices, uint capacity, out uint count);

    [DllImport("dualsense", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ds5_open(IntPtr context, ref Ds5DeviceInfo info, out IntPtr device);

    [DllImport("dualsense", CallingConvention = CallingConvention.Cdecl)]
    public static extern void ds5_close(IntPtr device);

    [DllImport("dualsense", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ds5_poll_state_timeout(IntPtr device, uint timeoutMs, ref Ds5State state);

    [DllImport("dualsense", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ds5_try_poll_state(IntPtr device, ref Ds5State state);

    [DllImport("dualsense", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ds5_reset_feedback(IntPtr device);

    [DllImport("dualsense", CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr ds5_get_last_error();
}

public static class Program
{
    private static string LastError() => Marshal.PtrToStringAnsi(DualSenseNative.ds5_get_last_error()) ?? "unknown";

    public static unsafe int Main()
    {
        int result = DualSenseNative.ds5_init(out IntPtr context);
        if (result != (int)Ds5Result.Ok)
        {
            Console.Error.WriteLine(LastError());
            return 1;
        }

        try
        {
            result = DualSenseNative.ds5_enumerate(context, null, 0, out uint count);
            if (result != (int)Ds5Result.Ok && result != (int)Ds5Result.InsufficientBuffer)
            {
                Console.Error.WriteLine(LastError());
                return 1;
            }

            Console.WriteLine($"DualSense controllers found: {count}");
            if (count == 0)
            {
                return 0;
            }

            var devices = new Ds5DeviceInfo[count];
            for (int i = 0; i < devices.Length; ++i)
            {
                DualSenseNative.ds5_device_info_init(ref devices[i]);
            }

            result = DualSenseNative.ds5_enumerate(context, devices, count, out count);
            if (result != (int)Ds5Result.Ok)
            {
                Console.Error.WriteLine(LastError());
                return 1;
            }

            result = DualSenseNative.ds5_open(context, ref devices[0], out IntPtr device);
            if (result != (int)Ds5Result.Ok)
            {
                Console.Error.WriteLine(LastError());
                return 1;
            }

            try
            {
                var state = new Ds5State();
                DualSenseNative.ds5_state_init(ref state);
                result = DualSenseNative.ds5_poll_state_timeout(device, 16, ref state);
                if (result == (int)Ds5Result.Ok)
                {
                    Console.WriteLine($"buttons=0x{state.buttons:x8} L2={state.left_trigger} R2={state.right_trigger}");
                }
                else if (result == (int)Ds5Result.Timeout)
                {
                    Console.WriteLine("No input report ready within 16ms");
                }
                else
                {
                    Console.Error.WriteLine(LastError());
                    return 1;
                }

                DualSenseNative.ds5_reset_feedback(device);
            }
            finally
            {
                DualSenseNative.ds5_close(device);
            }
        }
        finally
        {
            DualSenseNative.ds5_shutdown(context);
        }

        return 0;
    }
}
