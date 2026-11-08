using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;
using System.Windows.Forms;

static class Entry {
    static readonly string[] PackageFiles={"usb_bridge.inf","usb_bridge.cat","usb_bridge_test.cer","install.ps1","uninstall.ps1"};

    static Stream OpenPackageFile(string name) {
        Stream resource=typeof(Entry).Assembly.GetManifestResourceStream(name);
        if(resource==null) throw new InvalidDataException("Missing embedded package file: "+name);
        return resource;
    }

    public static void CheckPackage() {
        foreach(string file in PackageFiles) {
            using(Stream resource=OpenPackageFile(file)) {
                if(resource.Length==0) throw new InvalidDataException("Empty embedded package file: "+file);
            }
        }
        string inf;
        using(var reader=new StreamReader(OpenPackageFile("usb_bridge.inf"))) inf=reader.ReadToEnd();
        foreach(string value in new[]{"USB\\VID_1A86&PID_0001","USB\\VID_1A86&PID_0002","USB UART","USB RS485","usbser.inf"})
            if(!inf.Contains(value)) throw new Exception("Unexpected INF configuration: "+value);
    }

    public static void ExtractPackage(string directory) {
        foreach(string file in PackageFiles) {
            using(Stream resource=OpenPackageFile(file))
            using(var output=new FileStream(Path.Combine(directory,file),FileMode.CreateNew,FileAccess.Write))
                resource.CopyTo(output);
        }
    }

    [STAThread] static int Main(string[] args) {
        if(args.Length>0 && args[0]=="--selftest") {
            CheckPackage();
            Console.WriteLine("PASS: embedded installer files and VID/PID/name mapping; no system changes");
            return 0;
        }
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new Installer());
        return 0;
    }
}
class Installer : Form {
    readonly Button install=new Button();
    readonly Button uninstall=new Button();
    bool packageReady;
    bool removing;
    readonly TextBox log=new TextBox();
    readonly BackgroundWorker worker=new BackgroundWorker();
    public Installer() {
        Text="USB UART / RS485 Driver Setup";
        ClientSize=new Size(650,300); StartPosition=FormStartPosition.CenterScreen;
        Font=new Font("Microsoft YaHei UI",10); FormBorderStyle=FormBorderStyle.FixedDialog; MaximizeBox=false;
        var title=new Label{Text="USB UART / RS485",Font=new Font(Font.FontFamily,18,FontStyle.Bold),AutoSize=true,Location=new Point(22,18)};
        Controls.Add(title);
        install.Text="安装驱动"; install.SetBounds(24,70,150,38); install.Enabled=false;
        install.Click+=delegate { StartInstall(); }; Controls.Add(install);
        uninstall.Text="卸载驱动"; uninstall.SetBounds(190,70,150,38);
        uninstall.Click+=delegate {StartUninstall();}; Controls.Add(uninstall);
        var close=new Button{Text="关闭",Location=new Point(480,70),Size=new Size(145,38)};
        close.Click+=delegate {Close();}; Controls.Add(close);
        log.SetBounds(24,125,601,150); log.Multiline=true; log.ReadOnly=true; log.ScrollBars=ScrollBars.Vertical; Controls.Add(log);
        worker.DoWork+=delegate(object sender,DoWorkEventArgs e) {
            e.Result=RunDriverScript();
        };
        worker.RunWorkerCompleted+=delegate(object sender,RunWorkerCompletedEventArgs e) {
            install.Enabled=packageReady; uninstall.Enabled=true;
            if(e.Error!=null) {Append(e.Error.Message);return;}
            Append((int)e.Result==0?(removing?"卸载完成。请拔插设备，确认已恢复系统驱动。":"安装程序执行完成。请拔插设备，检查实际设备名称和 COM 口。"):(removing?"卸载失败，请查看日志。":"安装失败，请查看日志。"));
        };
        FormClosing+=delegate(object s,FormClosingEventArgs e) { if(worker.IsBusy) {e.Cancel=true;MessageBox.Show("请等待安装结束。 ");} };
        try {
            Entry.CheckPackage();
            packageReady=true;
            install.Enabled=true;
            Append("驱动包就绪。点击安装驱动后确认信任本包测试证书。");
        } catch(Exception e) {Append(e.Message);}
    }

    int RunDriverScript() {
        // Stage the elevated scripts under the system-protected Program Files directory.
        string stagingRoot=Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
        if(string.IsNullOrEmpty(stagingRoot)) throw new IOException("Program Files directory is unavailable.");
        string directory=Path.Combine(stagingRoot,"USBBridgeDriver-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(directory);
        try {
            Entry.ExtractPackage(directory);
            string script=Path.Combine(directory,removing?"uninstall.ps1":"install.ps1");
            var start=new ProcessStartInfo(Path.Combine(Environment.SystemDirectory,"WindowsPowerShell\\v1.0\\powershell.exe"),
                "-NoProfile -ExecutionPolicy Bypass -File \""+script+"\""+(removing?"":" -TrustLocalTestCertificate")) {
                WorkingDirectory=directory,UseShellExecute=false,CreateNoWindow=true,
                RedirectStandardOutput=true,RedirectStandardError=true,
                StandardOutputEncoding=Encoding.UTF8,StandardErrorEncoding=Encoding.UTF8
            };
            using(var p=new Process{StartInfo=start}) {
                p.OutputDataReceived+=delegate(object s,DataReceivedEventArgs a){if(a.Data!=null) Append(a.Data);};
                p.ErrorDataReceived+=delegate(object s,DataReceivedEventArgs a){if(a.Data!=null) Append(a.Data);};
                p.Start(); p.BeginOutputReadLine(); p.BeginErrorReadLine(); p.WaitForExit();
                return p.ExitCode;
            }
        } finally {
            try {Directory.Delete(directory,true);}
            catch(IOException e) {Append("无法清理临时目录 "+directory+"。"+e.Message);}
            catch(UnauthorizedAccessException e) {Append("无法清理临时目录 "+directory+"。"+e.Message);}
        }
    }

    void Append(string value) {
        if(InvokeRequired){BeginInvoke(new Action<string>(Append),value);return;}
        log.AppendText(value+Environment.NewLine);
    }
    void StartInstall() {
        if(!packageReady || worker.IsBusy) return;
        if(MessageBox.Show("将把 USB Bridge Local Test 证书加入本机受信任根和受信任发布者，并安装驱动包。\n这不是微软正式厂商签名包。是否继续？","确认安装",MessageBoxButtons.YesNo,MessageBoxIcon.Question)!=DialogResult.Yes)return;
        removing=false;install.Enabled=false;uninstall.Enabled=false;log.Clear();worker.RunWorkerAsync();
    }
    void StartUninstall() {
        if(worker.IsBusy) return;
        if(MessageBox.Show("卸载本工程的驱动包并移除本包测试证书信任？\n连接中的设备可能断开。系统 usbser.sys 将保留。","确认卸载",MessageBoxButtons.YesNo,MessageBoxIcon.Question)!=DialogResult.Yes) return;
        removing=true;install.Enabled=false;uninstall.Enabled=false;log.Clear();worker.RunWorkerAsync();
    }

}
