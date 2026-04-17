import fitz  # PyMuPDF
import os
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
import threading
from tkinterdnd2 import DND_FILES, TkinterDnD

class PDFBatchConverterApp:
    def __init__(self, root):
        self.root = root
        self.root.title("PDF 转图片 (智能路径版)")
        self.root.geometry("720x650")
        
        # 变量
        self.pdf_files = []
        self.img_format = tk.StringVar(value="png")   # 默认 png
        self.quality_mode = tk.StringVar(value="标准 (2x) - 推荐")
        self.custom_save_path = tk.StringVar(value="") 
        self.is_running = False

        # 配置拖拽
        self.root.drop_target_register(DND_FILES)
        self.root.dnd_bind('<<Drop>>', self.drop_files)

        self.create_widgets()
        
        # 绑定路径输入框变化，实时更新提示
        self.custom_save_path.trace("w", lambda *args: self.update_path_preview())

    def create_widgets(self):
        # 1. 顶部提示
        top_frame = tk.Frame(self.root)
        top_frame.pack(pady=10)
        tk.Label(top_frame, text="👇 拖拽 PDF 到此处 👇", font=("微软雅黑", 14, "bold"), fg="#333").pack()

        # 2. 列表区域
        list_frame = tk.Frame(self.root)
        list_frame.pack(pady=5, fill=tk.BOTH, expand=True, padx=20)
        
        scrollbar = tk.Scrollbar(list_frame)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        self.file_listbox = tk.Listbox(list_frame, selectmode=tk.EXTENDED, height=8, 
                                       yscrollcommand=scrollbar.set, font=("Consolas", 10))
        self.file_listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.config(command=self.file_listbox.yview)

        # 3. 设置区域
        setting_frame = tk.LabelFrame(self.root, text=" ⚙️ 转换参数 ", font=("微软雅黑", 10, "bold"), fg="#0078D7")
        setting_frame.pack(pady=5, fill=tk.X, padx=20, ipady=5)

        # 3.1 按钮行
        btn_row = tk.Frame(setting_frame)
        btn_row.pack(fill=tk.X, padx=10, pady=5)
        tk.Button(btn_row, text="➕ 添加文件", command=self.select_pdfs).pack(side=tk.LEFT, padx=5)
        tk.Button(btn_row, text="⛔ 清空列表", command=self.clear_list).pack(side=tk.LEFT, padx=5)

        # 3.2 选项行
        opt_row = tk.Frame(setting_frame)
        opt_row.pack(fill=tk.X, padx=10, pady=5)
        
        tk.Label(opt_row, text="输出格式：").pack(side=tk.LEFT)
        tk.Radiobutton(opt_row, text="PNG (默认)", variable=self.img_format, value="png").pack(side=tk.LEFT)
        tk.Radiobutton(opt_row, text="JPG (体积小)", variable=self.img_format, value="jpg").pack(side=tk.LEFT, padx=10)

        tk.Label(opt_row, text="清晰度：").pack(side=tk.LEFT, padx=(20, 0))
        combo_quality = ttk.Combobox(opt_row, textvariable=self.quality_mode, state="readonly", width=18)
        combo_quality['values'] = ('极速 (1x)', '标准 (2x) - 推荐', '高清 (3x)')
        combo_quality.pack(side=tk.LEFT)

        # 4. 保存路径设置区域
        path_frame = tk.LabelFrame(self.root, text=" 📂 保存位置 ", font=("微软雅黑", 10, "bold"), fg="#E67E22")
        path_frame.pack(pady=5, fill=tk.X, padx=20, ipady=5)
        
        path_inner = tk.Frame(path_frame)
        path_inner.pack(fill=tk.X, padx=10)

        self.entry_path = tk.Entry(path_inner, textvariable=self.custom_save_path, width=50, fg="#555")
        self.entry_path.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
        
        tk.Button(path_inner, text="浏览...", command=self.browse_save_dir).pack(side=tk.LEFT, padx=2)
        tk.Button(path_inner, text="重置为原目录", command=self.reset_save_dir).pack(side=tk.LEFT, padx=2)
        
        # 【关键修改】路径说明标签，字体加粗变色
        self.lbl_path_hint = tk.Label(path_frame, text="提示：留空则自动保存在 PDF 所在的文件夹", font=("微软雅黑", 9), fg="gray")
        self.lbl_path_hint.pack(anchor="w", padx=15, pady=(5,0))

        # 5. 进度条
        self.progress_frame = tk.Frame(self.root)
        self.progress_frame.pack(pady=5, fill=tk.X, padx=20)
        
        self.lbl_status = tk.Label(self.progress_frame, text="准备就绪", font=("微软雅黑", 10), anchor="w")
        self.lbl_status.pack(fill=tk.X)
        
        self.progress = ttk.Progressbar(self.progress_frame, orient="horizontal", length=600, mode="determinate")
        self.progress.pack(pady=5, fill=tk.X)

        # 6. 开始按钮
        self.btn_start = tk.Button(self.root, text="🚀 开始转换", command=self.start_thread, 
                                   bg="#0078D7", fg="white", font=("微软雅黑", 12, "bold"))
        self.btn_start.pack(pady=10, ipadx=40, ipady=5)
        
        self.file_listbox.bind('<Delete>', self.delete_selected)
        
        # 初始化一下提示
        self.update_path_preview()

    # ------------------ 智能路径显示逻辑 (核心修改) ------------------

    def update_path_preview(self):
        """根据输入框内容 + 文件列表，智能显示提示信息"""
        custom_path = self.custom_save_path.get().strip()
        
        # 情况1：用户手动指定了路径 -> 优先级最高
        if custom_path:
            self.lbl_path_hint.config(text=f"✅ 强制保存到：{custom_path}", fg="#009900") # 绿色
            return

        # 情况2：没指定路径，且没有文件
        if not self.pdf_files:
            self.lbl_path_hint.config(text="ℹ️ 提示：留空则自动保存在 PDF 所在的文件夹", fg="gray")
            return

        # 情况3：没指定路径，但有文件 -> 自动计算源目录
        dirs = {os.path.dirname(f) for f in self.pdf_files}
        
        if len(dirs) == 1:
            # 所有文件都在同一个目录
            single_path = list(dirs)[0]
            self.lbl_path_hint.config(text=f"📂 默认保存位置：{single_path}", fg="#005A9E") # 蓝色
        else:
            # 文件来自多个目录
            self.lbl_path_hint.config(text="📂 默认保存位置：分别保存在各自的源文件夹内", fg="#005A9E")

    def browse_save_dir(self):
        directory = filedialog.askdirectory()
        if directory:
            self.custom_save_path.set(directory)
            # trace 会自动触发 update_path_preview

    def reset_save_dir(self):
        self.custom_save_path.set("")
        # trace 会自动触发 update_path_preview

    # ------------------ 文件列表逻辑 ------------------

    def drop_files(self, event):
        files = self.root.tk.splitlist(event.data)
        self.add_files(files)

    def select_pdfs(self):
        filenames = filedialog.askopenfilenames(filetypes=[("PDF Files", "*.pdf")])
        self.add_files(filenames)

    def add_files(self, files):
        changed = False
        for f in files:
            if f.lower().endswith('.pdf') and f not in self.pdf_files:
                self.pdf_files.append(f)
                self.file_listbox.insert(tk.END, f)
                changed = True
        
        if changed:
            self.update_path_preview() # 添加文件后刷新提示

    def delete_selected(self, event):
        selection = self.file_listbox.curselection()
        if selection:
            for index in reversed(selection):
                self.file_listbox.delete(index)
                del self.pdf_files[index]
            self.update_path_preview() # 删除文件后刷新提示

    def clear_list(self):
        self.pdf_files.clear()
        self.file_listbox.delete(0, tk.END)
        self.update_path_preview() # 清空后刷新提示

    # ------------------ 转换核心逻辑 ------------------

    def start_thread(self):
        if not self.pdf_files:
            messagebox.showwarning("提示", "请先添加 PDF 文件！")
            return
        if self.is_running:
            return
        
        self.is_running = True
        self.btn_start.config(state=tk.DISABLED, text="正在处理...")
        threading.Thread(target=self.convert, daemon=True).start()

    def convert(self):
        try:
            total_files = len(self.pdf_files)
            fmt = self.img_format.get()
            q_mode = self.quality_mode.get()
            user_save_dir = self.custom_save_path.get().strip()

            zoom = 2.0
            if "极速" in q_mode: zoom = 1.0
            elif "高清" in q_mode: zoom = 3.0
            mat = fitz.Matrix(zoom, zoom)
            
            start_time = time.time()

            for index, pdf_file in enumerate(self.pdf_files):
                if not os.path.exists(pdf_file): continue

                base_name = os.path.splitext(os.path.basename(pdf_file))[0]
                
                # 确定输出目录
                if user_save_dir:
                    output_dir = user_save_dir
                    if not os.path.exists(output_dir):
                        try:
                            os.makedirs(output_dir, exist_ok=True)
                        except:
                            pass
                else:
                    output_dir = os.path.dirname(pdf_file)

                # 更新 UI
                self.root.after(0, lambda i=index, n=base_name: self.lbl_status.config(
                    text=f"[{i+1}/{total_files}] 正在转换: {n}..."
                ))

                doc = fitz.open(pdf_file)
                page_count = doc.page_count
                
                self.root.after(0, lambda m=page_count: self.progress.configure(maximum=m, value=0))

                for page_num in range(page_count):
                    page = doc.load_page(page_num)
                    pix = page.get_pixmap(matrix=mat, alpha=False)
                    
                    file_name = f"page{page_num+1:03d}_{base_name}.{fmt}"
                    save_path = os.path.join(output_dir, file_name)
                    
                    if fmt == "jpg":
                        pix.save(save_path, output="jpg", jpg_quality=90)
                    else:
                        pix.save(save_path)
                    
                    if page_num % 5 == 0 or page_num == page_count - 1:
                        self.root.after(0, lambda v=page_num+1: self.progress.configure(value=v))

                doc.close()
            
            end_time = time.time()
            duration = end_time - start_time
            self.root.after(0, lambda: self.finish_task(total_files, duration))

        except Exception as e:
            self.root.after(0, lambda: messagebox.showerror("错误", f"发生错误：{str(e)}"))
            self.root.after(0, lambda: self.reset_ui())

    def finish_task(self, count, duration):
        self.lbl_status.config(text=f"完成！耗时 {duration:.2f} 秒")
        self.progress["value"] = 0
        
        save_loc = self.custom_save_path.get().strip()
        msg_loc = save_loc if save_loc else "PDF 各自所在的文件夹"
        
        messagebox.showinfo("处理完成", f"成功转换 {count} 个文件！\n图片保存在：{msg_loc}")
        self.reset_ui()

    def reset_ui(self):
        self.is_running = False
        self.btn_start.config(state=tk.NORMAL, text="🚀 开始转换")

if __name__ == "__main__":
    root = TkinterDnD.Tk()
    app = PDFBatchConverterApp(root)
    root.mainloop()