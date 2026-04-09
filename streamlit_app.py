import os
import re
import subprocess
from datetime import datetime
from pathlib import Path

import streamlit as st
import streamlit.components.v1 as components


REPO_ROOT = Path(__file__).resolve().parent
DOCS_DIR = REPO_ROOT / "docs"
EXAMPLES_DIR = REPO_ROOT / "examples"
TMP_DIR = REPO_ROOT / ".streamlit_tmp"
BINARY_PATH = REPO_ROOT / ("cd_frontend.exe" if os.name == "nt" else "cd_frontend")

SVG_ARTIFACTS = [
    "token_stream.svg",
    "parse_table.svg",
    "first_set.svg",
    "follow_set.svg",
    "grammar.svg",
    "panic_recovery.svg",
    "symbol_table.svg",
    "ast.svg",
]

TEXT_ARTIFACTS = [
    "token_stream.txt",
    "parse_table.txt",
    "symbol_table.txt",
]


def apply_cyber_theme() -> None:
    st.set_page_config(page_title="Compiler Lab Studio", page_icon="C", layout="wide")
    st.markdown(
        """
        <style>
        @import url('https://fonts.googleapis.com/css2?family=Sora:wght@400;600;700;800&family=JetBrains+Mono:wght@400;600&display=swap');

        :root {
            --bg-ink: #091021;
            --bg-canvas: #0e1930;
            --bg-panel: rgba(13, 28, 52, 0.78);
            --line-soft: rgba(121, 228, 255, 0.22);
            --line-strong: rgba(121, 228, 255, 0.45);
            --text-main: #eaf6ff;
            --text-sub: #a9c9db;
            --accent-cyan: #6af2ff;
            --accent-mint: #7cf8cb;
            --accent-coral: #ff6b6b;
            --accent-gold: #ffd27a;
        }

        .stApp {
            background:
                radial-gradient(circle at 12% 12%, rgba(79, 224, 255, 0.22), rgba(0, 0, 0, 0) 42%),
                radial-gradient(circle at 86% 14%, rgba(84, 241, 178, 0.18), rgba(0, 0, 0, 0) 36%),
                linear-gradient(128deg, var(--bg-ink) 0%, #0b1730 48%, #122643 100%);
            color: var(--text-main);
            font-family: "Sora", "Segoe UI", sans-serif;
        }

        [data-testid="stSidebar"] {
            background: linear-gradient(180deg, #edf2f8 0%, #d9e1ee 100%);
            border-right: 1px solid rgba(32, 56, 88, 0.18);
        }

        [data-testid="stSidebar"] * {
            font-family: "Sora", "Segoe UI", sans-serif;
            color: #233149;
        }

        .control-title {
            letter-spacing: 0.02em;
            font-size: 0.9rem;
            font-weight: 700;
            text-transform: uppercase;
            margin-bottom: 8px;
            color: #344a68;
        }

        .hero {
            border: 1px solid var(--line-strong);
            border-radius: 18px;
            padding: 24px 24px 20px 24px;
            background:
                linear-gradient(160deg, rgba(6, 24, 44, 0.92) 0%, rgba(7, 20, 40, 0.88) 58%, rgba(14, 43, 68, 0.86) 100%);
            box-shadow: 0 14px 38px rgba(3, 12, 24, 0.36), inset 0 0 0 1px rgba(106, 242, 255, 0.1);
            margin-bottom: 16px;
        }

        .hero-badge {
            display: inline-flex;
            padding: 6px 11px;
            border-radius: 999px;
            font-size: 0.74rem;
            letter-spacing: 0.08em;
            text-transform: uppercase;
            font-weight: 700;
            color: #0d3a3c;
            background: linear-gradient(90deg, var(--accent-cyan), var(--accent-mint));
            margin-bottom: 12px;
        }

        .hero h1 {
            margin: 0;
            font-size: 2.15rem;
            letter-spacing: 0.01em;
            color: #dff6ff;
            line-height: 1.1;
        }

        .hero p {
            margin: 10px 0 16px 0;
            color: var(--text-sub);
            font-size: 1.01rem;
        }

        .hero-grid {
            display: grid;
            grid-template-columns: repeat(3, minmax(0, 1fr));
            gap: 10px;
        }

        .hero-kpi {
            border: 1px solid var(--line-soft);
            border-radius: 12px;
            padding: 10px 12px;
            background: rgba(11, 35, 57, 0.64);
        }

        .hero-kpi span {
            display: block;
            color: #9ac5db;
            font-size: 0.75rem;
            letter-spacing: 0.06em;
            text-transform: uppercase;
        }

        .hero-kpi strong {
            font-size: 1.2rem;
            color: #f5fbff;
            font-weight: 700;
        }

        .status-chip {
            border: 1px solid var(--line-soft);
            border-radius: 12px;
            padding: 12px 14px;
            margin: 8px 0 10px 0;
            background: rgba(12, 31, 54, 0.74);
        }

        .status-chip span {
            display: block;
            font-size: 0.78rem;
            letter-spacing: 0.08em;
            text-transform: uppercase;
            color: #9fcbe0;
            margin-bottom: 4px;
        }

        .status-ok {
            color: var(--accent-mint);
            font-weight: 700;
        }

        .status-bad {
            color: #ff9a9a;
            font-weight: 700;
        }

        .status-warn {
            color: var(--accent-gold);
            font-weight: 700;
        }

        .stButton > button {
            border-radius: 11px;
            border: 1px solid rgba(255, 255, 255, 0.14);
            background: linear-gradient(120deg, #ff7a59 0%, #ff5a6d 45%, #ef476f 100%);
            color: #fff;
            font-family: "Sora", "Segoe UI", sans-serif;
            font-weight: 700;
            letter-spacing: 0.01em;
            padding: 0.58rem 1.1rem;
            box-shadow: 0 9px 18px rgba(239, 71, 111, 0.3);
            transition: transform 0.12s ease, box-shadow 0.12s ease, filter 0.12s ease;
        }

        .stButton > button:hover {
            transform: translateY(-1px);
            filter: saturate(1.08);
            box-shadow: 0 12px 24px rgba(239, 71, 111, 0.38);
        }

        .stTextArea textarea, .stTextInput input {
            border-radius: 12px;
            border: 1px solid rgba(255, 255, 255, 0.16);
            background: #f2f5f9;
            color: #0f1f34;
            font-family: "JetBrains Mono", "Consolas", monospace;
            font-size: 0.95rem;
        }

        .stTextArea textarea:focus {
            border-color: rgba(106, 242, 255, 0.72);
            box-shadow: 0 0 0 1px rgba(106, 242, 255, 0.34);
        }

        .stTabs [data-baseweb="tab-list"] {
            gap: 10px;
            margin-top: 6px;
        }

        .stTabs [data-baseweb="tab"] {
            border: 1px solid var(--line-soft);
            border-radius: 11px;
            background: var(--bg-panel);
            color: #ddf5ff;
            font-weight: 600;
            padding: 0.45rem 0.9rem;
        }

        .stTabs [aria-selected="true"] {
            border-color: rgba(106, 242, 255, 0.72) !important;
            box-shadow: inset 0 0 0 1px rgba(106, 242, 255, 0.32);
        }

        .streamlit-expanderHeader {
            border-radius: 10px;
            border: 1px solid var(--line-soft);
            background: rgba(15, 33, 57, 0.72);
        }

        .stMultiSelect [data-baseweb="select"] {
            border-radius: 12px;
        }

        .stCaption {
            color: #95bbcf;
        }

        @media (max-width: 900px) {
            .hero {
                padding: 18px 16px 16px 16px;
            }

            .hero h1 {
                font-size: 1.7rem;
            }

            .hero-grid {
                grid-template-columns: 1fr;
            }
        }
        </style>
        """,
        unsafe_allow_html=True,
    )


def init_state() -> None:
    defaults = {
        "run_done": False,
        "compile_log": "",
        "run_log": "",
        "return_code": None,
        "input_path": "",
        "source_code": "",
    }
    for key, value in defaults.items():
        if key not in st.session_state:
            st.session_state[key] = value


def collect_sources() -> list[str]:
    sources = [REPO_ROOT / "src" / "main.cpp"]
    sources.extend(sorted((REPO_ROOT / "src" / "compiler").glob("*.cpp")))
    return [str(src) for src in sources]


def compile_backend() -> tuple[bool, str]:
    cmd = [
        "g++",
        "-std=gnu++17",
        "-Iinclude",
        *collect_sources(),
        "-o",
        str(BINARY_PATH),
    ]
    proc = subprocess.run(cmd, cwd=REPO_ROOT, capture_output=True, text=True)
    log = (proc.stdout or "") + (proc.stderr or "")
    return proc.returncode == 0, log


def write_temp_input(code: str) -> Path:
    TMP_DIR.mkdir(parents=True, exist_ok=True)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    path = TMP_DIR / f"streamlit_input_{timestamp}.cd"
    path.write_text(code, encoding="utf-8")
    return path


def run_backend(input_path: Path) -> tuple[int, str]:
    proc = subprocess.run([str(BINARY_PATH), str(input_path)], cwd=REPO_ROOT, capture_output=True, text=True)
    log = (proc.stdout or "") + (proc.stderr or "")
    return proc.returncode, log


def read_text(path: Path) -> str:
    if not path.exists():
        return ""
    return path.read_text(encoding="utf-8", errors="replace")


def svg_frame_height(svg: str, default: int = 700) -> int:
    match = re.search(r'height="([0-9]+(?:\.[0-9]+)?)(pt|px)?"', svg, flags=re.IGNORECASE)
    if not match:
        return default

    raw = float(match.group(1))
    unit = (match.group(2) or "px").lower()
    px = raw * (4.0 / 3.0) if unit == "pt" else raw
    return max(320, min(int(px) + 24, 1600))


def render_svg_markup(svg: str) -> None:
    height = svg_frame_height(svg)
    try:
        components.html(svg, height=height, scrolling=True)
    except Exception:
        # Fallback for environments that block components rendering.
        st.markdown(svg, unsafe_allow_html=True)


def render_svg(path: Path) -> None:
    if not path.exists():
        st.info(f"Missing artifact: {path.name}")
        return
    svg = read_text(path)
    if "<svg" not in svg:
        st.warning(f"{path.name} does not contain SVG markup.")
        return
    render_svg_markup(svg)


def backend_status_message(return_code: int, run_log: str) -> tuple[str, str]:
    if return_code == 0:
        if "Parse Error:" in run_log or "Semantic Error:" in run_log:
            return "warn", "Backend completed with language errors (artifacts still generated where possible)."
        return "ok", "Backend completed successfully."
    return "bad", "Backend failed (non-zero return code)."


def app() -> None:
    apply_cyber_theme()
    init_state()

    existing_artifacts = [name for name in SVG_ARTIFACTS if (DOCS_DIR / name).exists()]
    existing_reports = [name for name in TEXT_ARTIFACTS if (DOCS_DIR / name).exists()]

    st.markdown(
        f"""
        <div class="hero">
            <div class="hero-badge">Hackathon Build</div>
            <h1>Compiler Lab Studio</h1>
            <p>Professional compiler observability dashboard for lexical, grammar, symbol, and AST artifacts.</p>
            <div class="hero-grid">
                <div class="hero-kpi"><span>SVG Artifacts</span><strong>{len(existing_artifacts)}</strong></div>
                <div class="hero-kpi"><span>Text Reports</span><strong>{len(existing_reports)}</strong></div>
                <div class="hero-kpi"><span>Backend</span><strong>{'Ready' if BINARY_PATH.exists() else 'Needs Build'}</strong></div>
            </div>
        </div>
        """,
        unsafe_allow_html=True,
    )

    with st.sidebar:
        st.header("Execution Controls")
        st.markdown('<div class="control-title">Source Mode</div>', unsafe_allow_html=True)
        mode = st.radio(
            "Source Mode",
            ["Type code", "Upload file", "Use example file"],
            index=0,
            label_visibility="collapsed",
        )
        st.markdown('<div class="control-title">Build Mode</div>', unsafe_allow_html=True)
        rebuild = st.checkbox("Rebuild backend before run", value=False)
        st.caption("Use rebuild when C++ sources changed or binary is missing.")

    code_text = ""
    input_display = ""

    if mode == "Type code":
        code_text = st.text_area(
            "Type code",
            value="x = 1\ny = 2\nprint(x + y)\n",
            height=280,
        )
        input_display = "Typed code"
    elif mode == "Upload file":
        uploaded = st.file_uploader("Upload source file", type=["cd", "txt", "py"])
        if uploaded is not None:
            code_text = uploaded.read().decode("utf-8", errors="replace")
            st.text_area("Uploaded preview", value=code_text, height=280)
            input_display = uploaded.name
    else:
        examples = sorted([p.name for p in EXAMPLES_DIR.glob("*.cd")])
        selected = st.selectbox("Select example", examples)
        selected_path = EXAMPLES_DIR / selected
        code_text = read_text(selected_path)
        st.text_area("Example preview", value=code_text, height=280)
        input_display = selected

    run_click = st.button("Compile and Run", type="primary")

    if run_click:
        if not code_text.strip():
            st.error("No input code provided.")
        else:
            if rebuild or not BINARY_PATH.exists():
                ok, compile_log = compile_backend()
                st.session_state["compile_log"] = compile_log
                if not ok:
                    st.session_state["run_done"] = False
                    st.error("Backend compilation failed. Check Compiler Log tab.")
                    return

            if mode == "Use example file":
                input_path = EXAMPLES_DIR / input_display
            else:
                input_path = write_temp_input(code_text)

            code, run_log = run_backend(input_path)
            st.session_state["run_done"] = True
            st.session_state["run_log"] = run_log
            st.session_state["return_code"] = code
            st.session_state["input_path"] = str(input_path)
            st.session_state["source_code"] = code_text

    run_done = bool(st.session_state["run_done"])
    run_log = st.session_state.get("run_log", "")

    if run_done:
        code = st.session_state["return_code"]
        level, message = backend_status_message(code, run_log)
        klass = "status-ok" if level == "ok" else ("status-warn" if level == "warn" else "status-bad")
        st.markdown(
            f'<div class="status-chip"><span>Pipeline Status</span><p class="{klass}">Status: {message}</p></div>',
            unsafe_allow_html=True,
        )
        st.caption(f"Input path used: {st.session_state['input_path']}")
    else:
        st.info("Compiler has not been run in this UI session. Showing existing artifacts from docs if available.")

    tab_visual, tab_reports, tab_logs = st.tabs(["SVG Outputs", "Text Reports", "Compiler Log"])

    with tab_visual:
        existing = [name for name in SVG_ARTIFACTS if (DOCS_DIR / name).exists()]
        if not existing:
            st.warning("No SVG artifacts found in docs. Run the compiler to generate them.")
        selected_svgs = st.multiselect("Select visual artifacts", SVG_ARTIFACTS, default=existing)
        for name in selected_svgs:
            with st.expander(f"Artifact: {name}", expanded=True):
                render_svg(DOCS_DIR / name)

    with tab_reports:
        report_name = st.selectbox("Select report", TEXT_ARTIFACTS)
        report_path = DOCS_DIR / report_name
        if report_path.exists():
            st.text_area(report_name, value=read_text(report_path), height=460)
        else:
            st.warning(f"Report not found: {report_name}")

    with tab_logs:
        compile_log = st.session_state.get("compile_log", "")
        if compile_log.strip():
            st.subheader("Compile log")
            st.text_area("Compile log text", value=compile_log, height=160)
        st.subheader("Run log")
        if run_done:
            st.text_area("Backend console output", value=run_log, height=340)
        else:
            st.info("No run log yet for this session. Click Run Compiler to execute the backend.")


if __name__ == "__main__":
    app()
