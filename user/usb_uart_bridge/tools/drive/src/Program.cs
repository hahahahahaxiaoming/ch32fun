// Adapted from D:/gitee/UART_CH570/TOOLS/Drive/src/Program.cs.
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Runtime.InteropServices;
using System.ServiceProcess;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows.Forms;
using System.Xml.Serialization;
using System.Security;
using Microsoft.Win32;

[assembly: System.Reflection.AssemblyTitle("USB Bridge Names")]
[assembly: System.Reflection.AssemblyProduct("USB Bridge Names")]
[assembly: System.Reflection.AssemblyVersion("1.1.1.0")]

public class SavedName { public string Id; public string Name; }
public class Device { public string Id, Port, Name, Kind; }
static class Program {
 public const string ServiceName="USBBridgeDeviceNamer";
 public const string TaskName="USBBridgeDeviceNamer-Events";
 public const string Channel="Microsoft-Windows-Kernel-PnP/Configuration";
 public static readonly string Folder=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),"USB Bridge Device Namer");
 public static readonly string Installed=Path.Combine(Folder,"USB_Bridge_Setup.exe");
 static readonly string Backup=Path.Combine(Folder,"original-names.xml");
 static readonly Guid Ports=new Guid("4d36e978-e325-11ce-bfc1-08002be10318");
 [StructLayout(LayoutKind.Sequential)] struct Info { public uint Size; public Guid Class; public uint DevInst; public IntPtr Reserved; }
 [DllImport("setupapi.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr SetupDiGetClassDevs(ref Guid g,string e,IntPtr p,uint flags);
 [DllImport("setupapi.dll",SetLastError=true)] static extern bool SetupDiEnumDeviceInfo(IntPtr h,uint n,ref Info i);
 [DllImport("setupapi.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool SetupDiGetDeviceInstanceId(IntPtr h,ref Info i,StringBuilder b,int size,out int needed);
 [DllImport("setupapi.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool SetupDiGetDeviceRegistryProperty(IntPtr h,ref Info i,uint p,out uint type,byte[] b,uint size,out uint needed);
 [DllImport("setupapi.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool SetupDiSetDeviceRegistryProperty(IntPtr h,ref Info i,uint p,byte[] b,uint size);
 [DllImport("setupapi.dll",SetLastError=true)] static extern IntPtr SetupDiOpenDevRegKey(IntPtr h,ref Info i,uint scope,uint profile,uint type,int access);
 [DllImport("advapi32.dll",CharSet=CharSet.Unicode)] static extern int RegQueryValueEx(IntPtr h,string name,IntPtr reserved,out uint type,byte[] data,ref uint size);
 [DllImport("advapi32.dll")] static extern int RegCloseKey(IntPtr h);
 [DllImport("setupapi.dll")] static extern bool SetupDiDestroyDeviceInfoList(IntPtr h);
 public static bool Match(string id) { return id!=null && Regex.IsMatch(id,@"\AUSB\\VID_1209&PID_D035\\(UART|RS485)-CH(570|591)-[0-9A-F]{12}\z",RegexOptions.IgnoreCase|RegexOptions.CultureInvariant); }
 [DllImport("cfgmgr32.dll")] static extern uint CM_Get_Parent(out uint parent,uint node,uint flags);
 [DllImport("cfgmgr32.dll",CharSet=CharSet.Unicode)] static extern uint CM_Get_Device_ID(uint node,StringBuilder id,int size,uint flags);
 static string Identity(string id,uint node) {
  for(int n=0;n<4;n++) {
   if(Match(id)) return id;
   uint parent; if(CM_Get_Parent(out parent,node,0)!=0) break;
   var b=new StringBuilder(1024); if(CM_Get_Device_ID(parent,b,b.Capacity,0)!=0) break;
   id=b.ToString(); node=parent;
  }
  return null;
 }
 public static string Kind(string id) { if(!Match(id)) return null; return id.IndexOf("\\RS485-",StringComparison.OrdinalIgnoreCase)>=0?"RS485":"UART"; }
 public static string TargetName(string kind,string port) { return "USB "+kind+" ("+port.ToUpperInvariant()+")"; }

 static string Property(IntPtr h,ref Info i,uint property) {
  byte[] b=new byte[8192]; uint t,n;
  if(SetupDiGetDeviceRegistryProperty(h,ref i,property,out t,b,(uint)b.Length,out n)) return Encoding.Unicode.GetString(b,0,(int)n).TrimEnd('\0');
  int error=Marshal.GetLastWin32Error(); if(error==13) return null; throw new Win32Exception(error);
 }
 static string Port(IntPtr h,ref Info i) {
  IntPtr key=SetupDiOpenDevRegKey(h,ref i,1,0,1,0x20019); if(key==new IntPtr(-1)) return null;
  try { byte[] b=new byte[512]; uint n=(uint)b.Length,t; return RegQueryValueEx(key,"PortName",IntPtr.Zero,out t,b,ref n)==0 ? Encoding.Unicode.GetString(b,0,(int)n).TrimEnd('\0') : null; } finally {RegCloseKey(key);}
 }
 delegate void Visit(IntPtr h,Info info,Device d);
 static void Enumerate(bool present,Visit visit) {
  Guid g=Ports; IntPtr h=SetupDiGetClassDevs(ref g,null,IntPtr.Zero,present?2u:0u);
  if(h==new IntPtr(-1)) throw new Win32Exception();
  try { for(uint n=0;;n++) { Info i=new Info(); i.Size=(uint)Marshal.SizeOf(typeof(Info));
   if(!SetupDiEnumDeviceInfo(h,n,ref i)) {int e=Marshal.GetLastWin32Error(); if(e==259) break; throw new Win32Exception(e);}
   StringBuilder b=new StringBuilder(1024); int needed;
   if(!SetupDiGetDeviceInstanceId(h,ref i,b,b.Capacity,out needed)) throw new Win32Exception();
   string identity=Identity(b.ToString(),i.DevInst); if(identity==null) continue;
   visit(h,i,new Device{Id=b.ToString(),Kind=Kind(identity),Port=Port(h,ref i),Name=Property(h,ref i,12)});
  }} finally {SetupDiDestroyDeviceInfoList(h);}
 }
 public static List<Device> Scan() {var result=new List<Device>(); Enumerate(true,delegate(IntPtr h,Info i,Device d){result.Add(d);}); return result;}
 static List<SavedName> Load() { if(!File.Exists(Backup)) return new List<SavedName>(); using(var f=File.OpenRead(Backup)) return (List<SavedName>)new XmlSerializer(typeof(List<SavedName>)).Deserialize(f); }
 static void Save(List<SavedName> names) {string tmp=Backup+".tmp"; using(var f=File.Create(tmp)) {new XmlSerializer(typeof(List<SavedName>)).Serialize(f,names); f.Flush(true);} if(File.Exists(Backup)) File.Replace(tmp,Backup,null); else File.Move(tmp,Backup);}
 static void Set(IntPtr h,Info i,string name) {byte[] b=name==null?null:Encoding.Unicode.GetBytes(name+"\0"); if(!SetupDiSetDeviceRegistryProperty(h,ref i,12,b,b==null?0u:(uint)b.Length)) throw new Win32Exception();}
 public static int Rename() {
  int count=0; var names=Load(); Enumerate(true,delegate(IntPtr h,Info i,Device d){
   if(d.Port==null || !Regex.IsMatch(d.Port,@"\ACOM[1-9][0-9]*\z",RegexOptions.IgnoreCase)) return;
   string target=TargetName(d.Kind,d.Port); if(d.Name==target) return;
   if(!names.Exists(x=>string.Equals(x.Id,d.Id,StringComparison.OrdinalIgnoreCase))) {names.Add(new SavedName{Id=d.Id,Name=d.Name}); Save(names);}
   Set(h,i,target); count++; Log("已改名："+d.Id+" → "+target);
  }); return count;
 }
 static void Restore() {var names=Load(); Enumerate(false,delegate(IntPtr h,Info i,Device d){var saved=names.Find(x=>string.Equals(x.Id,d.Id,StringComparison.OrdinalIgnoreCase)); if(saved==null) return;
   // Do not overwrite a name subsequently customized by another tool.
   if(d.Name!=null && Regex.IsMatch(d.Name,@"\AUSB (UART|RS485) \(COM[1-9][0-9]*\)\z",RegexOptions.IgnoreCase)) {string name=saved.Name; if(name!=null && d.Port!=null) name=Regex.Replace(name,@"\(COM[0-9]+\)","("+d.Port+")",RegexOptions.IgnoreCase); Set(h,i,name);}
  });}
 public static void Log(string text) {try {Directory.CreateDirectory(Folder); string f=Path.Combine(Folder,"activity.log"); if(File.Exists(f)&&new FileInfo(f).Length>1048576) File.Delete(f); File.AppendAllText(f,DateTime.Now.ToString("s")+" "+text+Environment.NewLine,Encoding.UTF8);}catch{}}
 static void Sc(string args) {using(var p=Process.Start(new ProcessStartInfo(Path.Combine(Environment.SystemDirectory,"sc.exe"),args){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true})) {string output=p.StandardOutput.ReadToEnd(); p.WaitForExit(); if(p.ExitCode!=0) throw new Exception("服务操作失败 ("+p.ExitCode+")："+output);}}
 static bool ServiceExists() {foreach(var s in ServiceController.GetServices()) {using(s) {if(s.ServiceName==ServiceName) return true;}} return false;}
 static void Stop() {if(!ServiceExists()) return; using(var s=new ServiceController(ServiceName)) {if(s.Status!=ServiceControllerStatus.Stopped){s.Stop(); s.WaitForStatus(ServiceControllerStatus.Stopped,TimeSpan.FromSeconds(30));}}}
 static int Command(string exe,string args,bool required) {using(var p=Process.Start(new ProcessStartInfo(Path.Combine(Environment.SystemDirectory,exe),args){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true})) {string output=p.StandardOutput.ReadToEnd(); p.WaitForExit(); if(required&&p.ExitCode!=0) throw new Exception(exe+" 操作失败 ("+p.ExitCode+")："+output); return p.ExitCode;}}
 static void RemoveTask() {
  if(Command("schtasks.exe","/Query /TN "+TaskName,false)==0) {
   Command("schtasks.exe","/Change /TN "+TaskName+" /Disable",true);
   Command("schtasks.exe","/End /TN "+TaskName,false);
   Command("schtasks.exe","/Delete /TN "+TaskName+" /F",true);
  }
 }
 public static string TaskXml() {
  string query="<QueryList><Query Id='0' Path='"+Channel+"'><Select Path='"+Channel+"'>*[System[(EventID=400 or EventID=410)] and EventData[Data[@Name='ClassGuid']='{4d36e978-e325-11ce-bfc1-08002be10318}' or Data[@Name='ClassGuid']='{4D36E978-E325-11CE-BFC1-08002BE10318}']]</Select></Query></QueryList>";
  return "<?xml version='1.0' encoding='utf-16'?><Task version='1.2' xmlns='http://schemas.microsoft.com/windows/2004/02/mit/task'>"+
   "<RegistrationInfo><Description>USB UART / RS485 设备命名：仅在开机、登录及串口配置/启动事件后运行，完成退出。</Description></RegistrationInfo>"+
   "<Triggers><BootTrigger><Enabled>true</Enabled><Delay>PT30S</Delay></BootTrigger><LogonTrigger><Enabled>true</Enabled><Delay>PT15S</Delay></LogonTrigger><EventTrigger><Enabled>true</Enabled><Subscription>"+SecurityElement.Escape(query)+"</Subscription><Delay>PT3S</Delay></EventTrigger></Triggers>"+
   "<Principals><Principal id='System'><UserId>S-1-5-18</UserId><RunLevel>HighestAvailable</RunLevel></Principal></Principals>"+
   "<Settings><MultipleInstancesPolicy>Queue</MultipleInstancesPolicy><DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries><StopIfGoingOnBatteries>false</StopIfGoingOnBatteries><StartWhenAvailable>true</StartWhenAvailable><ExecutionTimeLimit>PT2M</ExecutionTimeLimit><RestartOnFailure><Interval>PT1M</Interval><Count>2</Count></RestartOnFailure></Settings>"+
   "<Actions Context='System'><Exec><Command>"+SecurityElement.Escape(Installed)+"</Command><Arguments>--once</Arguments><WorkingDirectory>"+SecurityElement.Escape(Folder)+"</WorkingDirectory></Exec></Actions></Task>";
 }
 public static void Install() {
  Stop(); if(ServiceExists()) Sc("delete "+ServiceName); RemoveTask(); Directory.CreateDirectory(Folder); string current=Application.ExecutablePath;
  if(!string.Equals(current,Installed,StringComparison.OrdinalIgnoreCase)) File.Copy(current,Installed,true);
  Command("wevtutil.exe","sl \""+Channel+"\" /e:true",true);
  string taskFile=Path.Combine(Folder,"event-task.xml"); File.WriteAllText(taskFile,TaskXml(),Encoding.Unicode);
  Command("schtasks.exe","/Create /TN "+TaskName+" /XML \""+taskFile+"\" /F",true);
  int count=Rename();
  using(var key=Registry.LocalMachine.CreateSubKey(@"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\USBBridgeDeviceNamer")) {
   key.SetValue("DisplayName","USB Bridge Names"); key.SetValue("DisplayVersion","1.1.1"); key.SetValue("Publisher","USB Bridge Tools"); key.SetValue("InstallLocation",Folder); key.SetValue("UninstallString","\""+Installed+"\" --uninstall"); key.SetValue("DisplayIcon",Installed); key.SetValue("NoModify",1); key.SetValue("NoRepair",1);
  }
  MessageBox.Show("事件版安装完成。\n本次改名 "+count+" 台设备。\n开机、登录及串口配置/启动事件触发检查，处理完退出。\n没有常驻程序，也不定时轮询。\n\n如设备管理器仍显示旧名称，请刷新或重新打开。","USB Bridge",MessageBoxButtons.OK,MessageBoxIcon.Information);
 }
 public static void Uninstall() {
  RemoveTask(); Stop(); Restore(); if(ServiceExists()) Sc("delete "+ServiceName);
  Registry.LocalMachine.DeleteSubKeyTree(@"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\USBBridgeDeviceNamer",false);
  // The executing uninstall program cannot delete itself; retain backups and a reusable installer.
  MessageBox.Show("事件任务及旧版后台服务已移除，已登记设备的原名称已恢复。\n安装目录保留程序、日志及原名称备份：\n"+Folder,"USB Bridge");
 }
 [STAThread] static int Main(string[] args) {
  if(args.Length>0&&args[0]=="--selftest") return Tests();
  if(args.Length>0&&args[0]=="--export-task") {File.WriteAllText(args[1],TaskXml(),Encoding.Unicode); return 0;}
  if(args.Length>0&&args[0]=="--once") {
   try {int changed=0; for(int n=0;n<3;n++){if(n>0) Thread.Sleep(n==1?3000:7000); changed+=Rename();} Log("事件检查结束，改名 "+changed+" 台；进程退出。"); return 0;}catch(Exception e){Log(e.ToString());return 1;}
  }
  Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
  if(args.Length>0&&args[0]=="--uninstall") {try{Uninstall();}catch(Exception e){MessageBox.Show(e.Message,"卸载失败");} return 0;}
  Application.Run(new SetupForm()); return 0;
 }
 static int Tests() {
  string[] yes={@"USB\VID_1209&PID_D035\UART-CH570-001122334455",@"USB\VID_1209&PID_D035\RS485-CH591-AABBCCDDEEFF",@"usb\vid_1209&pid_d035\uart-ch591-aabbccddeeff"};
  string[] no={@"USB\VID_1A86&PID_7523\RS48500000000000001",@"USB\VID_1209&PID_D035\007",@"USB\VID_1209&PID_D035\UART-CH592-001122334455",@"USB\VID_1209&PID_D035\UART-CH570-00112233445",@"USB\VID_1209&PID_D035\UART-CH570-001122334455X",@"USB\VID_1209&PID_D035&MI_00\6&INSTANCE",null,""};
  foreach(string s in yes) if(!Match(s)) throw new Exception("False negative: "+s);
  foreach(string s in no) if(Match(s)) throw new Exception("False positive: "+s);
  if(Kind(yes[0])!="UART" || Kind(yes[1])!="RS485") throw new Exception("Mode mismatch");
  if(TargetName("UART","com3")!="USB UART (COM3)" || TargetName("RS485","COM4")!="USB RS485 (COM4)") throw new Exception("Name mismatch");
  Console.WriteLine("PASS: device identity rejection, mode and friendly name tests");
  foreach(var d in Scan()) Console.WriteLine(d.Id+" | "+d.Port+" | "+d.Name);
  Console.WriteLine("PASS: read-only SetupAPI enumeration (no devices modified)"); return 0;
 }
}
class SetupForm:Form {
 readonly TextBox output=new TextBox();
 public SetupForm(){Text="USB Bridge Names"; ClientSize=new Size(720,380); Font=new Font("Microsoft YaHei UI",10); StartPosition=FormStartPosition.CenterScreen; FormBorderStyle=FormBorderStyle.FixedDialog; MaximizeBox=false;
  var title=new Label{Text="USB Bridge Names",Font=new Font(Font.FontFamily,18,FontStyle.Bold),Location=new Point(24,20),AutoSize=true}; Controls.Add(title);
  output.SetBounds(25,75,670,210); output.Multiline=true; output.ReadOnly=true; output.ScrollBars=ScrollBars.Both; Controls.Add(output);
  Button install=ButtonAt("安装 / 修复",25,delegate{Program.Install();RefreshDevices();});
  ButtonAt("刷新设备",200,RefreshDevices); ButtonAt("卸载并恢复",375,delegate{Program.Uninstall();RefreshDevices();}); ButtonAt("关闭",550,Close);
  install.BackColor=Color.FromArgb(220,239,255); Shown+=delegate{RefreshDevices();};
 }
 Button ButtonAt(string text,int x,Action action){var b=new Button{Text=text,Location=new Point(x,310),Size=new Size(145,42)}; b.Click+=delegate{try{UseWaitCursor=true; action();}catch(Exception e){MessageBox.Show(e.Message,"操作失败",MessageBoxButtons.OK,MessageBoxIcon.Error);}finally{UseWaitCursor=false;}}; Controls.Add(b);return b;}
 void RefreshDevices(){try{var devices=Program.Scan(); var s=new StringBuilder(); s.AppendLine("当前匹配设备："+devices.Count+" 台（仅扫描，不修改）"); foreach(var d in devices) {s.AppendLine();s.AppendLine(d.Id); s.AppendLine((d.Port??"尚未分配 COM 口")+"    "+(d.Name??"无友好名称"));} if(devices.Count==0) s.AppendLine("请插入目标设备并安装Windows CDC 串口驱动，然后刷新。");output.Text=s.ToString();}catch(Exception e){output.Text="读取设备失败："+e.Message;}}
}
