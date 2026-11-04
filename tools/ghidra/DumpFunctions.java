import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import java.io.*;
public class DumpFunctions extends GhidraScript {
  public void run() throws Exception {
    String[] a = getScriptArgs();
    DecompInterface d = new DecompInterface(); d.openProgram(currentProgram);
    PrintWriter w = new PrintWriter(new FileWriter(a[0]));
    for (int i = 1; i < a.length; i++) {
      Address ad = toAddr(Long.parseLong(a[i], 16));
      Function f = getFunctionContaining(ad);
      if (f == null) f = createFunction(ad, null);
      w.println("//==== " + a[i]);
      if (f == null) { w.println("no function"); continue; }
      DecompileResults r = d.decompileFunction(f, 120, monitor);
      w.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "fail");
    }
    w.close();
  }
}
