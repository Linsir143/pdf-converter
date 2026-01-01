import streamlit as st
import fitz  # PyMuPDF
import zipfile
import io
import os

# 1. 网页基础配置
st.set_page_config(page_title="PDF 转图片神器", page_icon="📂", layout="wide")

# 2. 注入 CSS 样式 (大号拖拽框)
st.markdown("""
    <style>
    .main-title {
        font-size: 40px !important;
        font-weight: bold;
        color: #0078D7;
        text-align: center;
        margin-bottom: 20px;
    }
    section[data-testid="stFileUploaderDropzone"] {
        min-height: 250px;
        border: 2px dashed #0078D7;
        background-color: #f0f8ff;
        border-radius: 15px;
        align-items: center;
        justify-content: center;
    }
    section[data-testid="stFileUploaderDropzone"] div div::before {
        content: "👉 把所有 PDF 文件一次性拖到这里 (支持批量) 👈";
        font-size: 1.2em;
        font-weight: bold;
        color: #555;
    }
    </style>
""", unsafe_allow_html=True)

# 3. 页面标题
st.markdown('<div class="main-title">🚀 PDF 批量转图片工具</div>', unsafe_allow_html=True)

# 4. 侧边栏设置
with st.sidebar:
    st.header("⚙️ 参数设置")
    st.info("💡 提示：按住 Ctrl 或 Shift 可选择多个文件同时拖入")
    
    # 【修改处】把 png 放在第一个，index=0 表示默认选中第一个
    img_format = st.radio("输出格式", ["png", "jpg"], index=0, help="PNG (默认): 无损画质; JPG: 体积更小")
    
    quality_mode = st.selectbox("清晰度", ["极速 (1x)", "标准 (2x) - 推荐", "高清 (3x)"], index=1)

# 解析缩放比例
zoom = 2.0
if "极速" in quality_mode: zoom = 1.0
elif "高清" in quality_mode: zoom = 3.0

# 5. 上传区域
uploaded_files = st.file_uploader(
    label="上传区域", 
    type=["pdf"], 
    accept_multiple_files=True,
    help="请框选多个文件直接拖入"
)

# 6. 处理逻辑
if uploaded_files:
    total_files = len(uploaded_files)
    st.success(f"✅ 已成功识别 {total_files} 个文件！点击下方按钮开始转换。")
    
    col1, col2, col3 = st.columns([1, 2, 1])
    with col2:
        start_btn = st.button(f"🚀 开始转换 {total_files} 个 PDF", use_container_width=True, type="primary")

    if start_btn:
        zip_buffer = io.BytesIO()
        progress_text = st.empty()
        progress_bar = st.progress(0)
        
        with zipfile.ZipFile(zip_buffer, "w", zipfile.ZIP_DEFLATED) as zf:
            for i, pdf_file in enumerate(uploaded_files):
                try:
                    current_name = pdf_file.name
                    progress_text.text(f"正在处理 ({i+1}/{total_files}): {current_name} ...")
                    
                    pdf_bytes = pdf_file.read()
                    doc = fitz.open(stream=pdf_bytes, filetype="pdf")
                    mat = fitz.Matrix(zoom, zoom)
                    base_name = os.path.splitext(current_name)[0]

                    for page_num in range(doc.page_count):
                        page = doc.load_page(page_num)
                        # alpha=False 强制白底 (PNG也建议白底，除非你需要透明)
                        pix = page.get_pixmap(matrix=mat, alpha=False)
                        
                        # 图片转字节
                        if img_format == "jpg":
                            img_data = pix.tobytes(output="jpg", jpg_quality=90)
                        else:
                            img_data = pix.tobytes(output="png")
                        
                        file_name_in_zip = f"{base_name}/page{page_num+1:03d}_{base_name}.{img_format}"
                        zf.writestr(file_name_in_zip, img_data)
                    
                    doc.close()
                except Exception as e:
                    st.error(f"文件 {pdf_file.name} 处理失败: {e}")
                
                progress_bar.progress((i + 1) / total_files)

        progress_bar.progress(100)
        progress_text.text("🎉 全部转换完成！")
        
        st.balloons()
        st.markdown("### 👇 点击下载结果")
        st.download_button(
            label=f"📦 下载压缩包 ({total_files}个文件).zip",
            data=zip_buffer.getvalue(),
            file_name="pdf_images_converted.zip",
            mime="application/zip",
            type="primary"
        )