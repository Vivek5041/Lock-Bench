import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.Platform;

public class CpuAffinity {
    public interface CLibrary extends Library {
        CLibrary INSTANCE = Native.load(Platform.isWindows() ? "msvcrt" : "c", CLibrary.class);
        
        int sched_setaffinity(int pid, int cpusetsize, byte[] cpuset);
        int sched_getaffinity(int pid, int cpusetsize, byte[] cpuset);
    }

    public static void setAffinity(int core) {
        if (!Platform.isLinux()) {
            System.err.println("CPU affinity setting is only supported on Linux");
            return;
        }

        // Create CPU set - size is rounded up to multiple of 64 bits
        int cpuSetSize = (core / 64 + 1) * 8;
        byte[] cpuset = new byte[cpuSetSize];

        // Set the bit that represents the CPU core
        cpuset[core / 8] |= (1 << (core % 8));

        // Set the affinity
        int result = CLibrary.INSTANCE.sched_setaffinity(0, cpuSetSize, cpuset);
        if (result != 0) {
            System.err.println("Failed to set CPU affinity");
        }
    }
}
