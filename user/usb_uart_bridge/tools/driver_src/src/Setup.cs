using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;
using System.Windows.Forms;

static class Entry {
    public static string Root = AppDomain.CurrentDomain.BaseDirectory;
    public static void CheckPackage(string root) {
        foreach (string file in new[]{"usb_bridge.inf","usb_bridge.cat","usb_bridge_test.cer","install.ps1","uninstall.ps1"})
            if (!File.Exists(Path.Combine(root,file))) throw new Exception("Missing package file: "+file);
        string inf=File.ReadAllText(Path.Combine(root,"usb_bridge.inf"));
        foreach(string value in new[]{"USB\\VID_1A86&PID_0001","USB\\VID_1A86&PID_0002","USB UART","USB RS485","usbser.inf"})
            if(!inf.Contains(value)) throw new Exception("Unexpected INF configuration: "+value);
    }
    [STAThread] static int Main(string[] args) {
        if(args.Length>0 && args[0]=="--selftest") {
            CheckPackage(args.Length>1?args[1]:Root);
            Console.WriteLine("PASS: installer package files and VID/PID/name mapping; no system changes");
            return 0;
        }
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new Installer());
        return 0;
    }
}
class Installer : Form {
    readonly CheckBox consent=new CheckBox();
    readonly Button install=new Button();
    readonly Button uninstall=new Button();
    bool removing;
    readonly TextBox log=new TextBox();
    readonly BackgroundWorker worker=new BackgroundWorker();
    public Installer() {
        Text="USB UART / RS485 Driver Setup";
        ClientSize=new Size(650,340); StartPosition=FormStartPosition.CenterScreen;
        Font=new Font("Microsoft YaHei UI",10); FormBorderStyle=FormBorderStyle.FixedDialog; MaximizeBox=false;
        var title=new Label{Text="USB UART / RS485",Font=new Font(Font.FontFamily,18,FontStyle.Bold),AutoSize=true,Location=new Point(22,18)};
        Controls.Add(title);
        consent.Text="我同意在本机信任本包测试证书并安装驱动";
        consent.SetBounds(24,70,600,28); Controls.Add(consent);
        install.Text="安装驱动"; install.SetBounds(24,110,150,38); install.Enabled=false;
        install.Click+=delegate { StartInstall(); }; Controls.Add(install);
        uninstall.Text="卸载驱动"; uninstall.SetBounds(190,110,150,38);
        uninstall.Click+=delegate {StartUninstall();}; Controls.Add(uninstall);
        var close=new Button{Text="关闭",Location=new Point(480,110),Size=new Size(145,38)};
        close.Click+=delegate {Close();}; Controls.Add(close);
        log.SetBounds(24,165,601,150); log.Multiline=true; log.ReadOnly=true; log.ScrollBars=ScrollBars.Vertical; Controls.Add(log);
        consent.CheckedChanged+=delegate {install.Enabled=consent.Checked && !worker.IsBusy;};
        worker.DoWork+=delegate(object sender,DoWorkEventArgs e) {
            if(!removing) Entry.CheckPackage(Entry.Root);
            string script=removing?"uninstall.ps1":"install.ps1";
            if(!File.Exists(Path.Combine(Entry.Root,script))) throw new Exception("Missing script: "+script);
            var start=new ProcessStartInfo(Path.Combine(Environment.SystemDirectory,"WindowsPowerShell\\v1.0\\powershell.exe"),
                "-NoProfile -ExecutionPolicy Bypass -File \""+Path.Combine(Entry.Root,script)+"\""+(removing?"":" -TrustLocalTestCertificate")) {
                UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true,
                StandardOutputEncoding=Encoding.UTF8,StandardErrorEncoding=Encoding.UTF8
            };
            using(var p=new Process{StartInfo=start}) {
                p.OutputDataReceived+=delegate(object s,DataReceivedEventArgs a){if(a.Data!=null) Append(a.Data);};
                p.ErrorDataReceived+=delegate(object s,DataReceivedEventArgs a){if(a.Data!=null) Append(a.Data);};
                p.Start(); p.BeginOutputReadLine(); p.BeginErrorReadLine(); p.WaitForExit(); e.Result=p.ExitCode;
            }
        };
        worker.RunWorkerCompleted+=delegate(object sender,RunWorkerCompletedEventArgs e) {
            consent.Enabled=true; install.Enabled=consent.Checked; uninstall.Enabled=true;
            if(e.Error!=null) {Append(e.Error.Message);return;}
            Append((int)e.Result==0?(removing?"卸载完成。请拔插设备，确认已恢复系统驱动。":"安装程序执行完成。请拔插设备，检查实际设备名称和 COM 口。"):(removing?"卸载失败，请查看日志。":"安装失败，请查看日志。"));
        };
        FormClosing+=delegate(object s,FormClosingEventArgs e) { if(worker.IsBusy) {e.Cancel=true;MessageBox.Show("请等待安装结束。 ");} };
        try {Entry.CheckPackage(Entry.Root);Append("驱动包就绪。请阅读并勾选证书信任选项，再点击安装。");}
        catch(Exception e){Append(e.Message);consent.Enabled=false;}
    }
    void Append(string value) {
        if(InvokeRequired){BeginInvoke(new Action<string>(Append),value);return;}
        log.AppendText(value+Environment.NewLine);
    }
    void StartInstall() {
        if(!consent.Checked || worker.IsBusy) return;
        if(MessageBox.Show("将把 USB Bridge Local Test 证书加入本机受信任根和受信任发布者，并安装驱动包。\n这不是微软正式厂商签名包。是否继续？","确认安装",MessageBoxButtons.YesNo,MessageBoxIcon.Question)!=DialogResult.Yes)return;
        removing=false;install.Enabled=false;uninstall.Enabled=false;consent.Enabled=false;log.Clear();worker.RunWorkerAsync();
    }
    void StartUninstall() {
        if(worker.IsBusy) return;
        if(MessageBox.Show("卸载本工程的驱动包并移除本包测试证书信任？\n连接中的设备可能断开。系统 usbser.sys 将保留。","确认卸载",MessageBoxButtons.YesNo,MessageBoxIcon.Question)!=DialogResult.Yes) return;
        removing=true;install.Enabled=false;uninstall.Enabled=false;consent.Enabled=false;log.Clear();worker.RunWorkerAsync();
    }

}
