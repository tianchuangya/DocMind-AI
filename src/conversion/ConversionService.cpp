#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <sstream>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <optional>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <fstream>
#include <shared_mutex>
#include <regex>
#include <codecvt>
#include <locale>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(p,m) _mkdir(p)
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#endif

namespace ConversionService {

enum class Format { Markdown, DOCX, HTML, PDF, Unknown };
enum class TaskStatus { Pending, Running, Completed, Failed, Cancelled };
using TaskHandle = uint64_t;

enum class ConversionError {
    None, ToolMissing, SourceNotFound, UnsupportedFormat,
    CorruptFile, PasswordProtected, ScannedPdfNoOcr,
    Timeout, Cancelled, Unknown,
};

std::string error_str(ConversionError e) {
    switch(e) {
        case ConversionError::None: return "None";
        case ConversionError::ToolMissing: return "ToolMissing";
        case ConversionError::SourceNotFound: return "SourceNotFound";
        case ConversionError::UnsupportedFormat: return "UnsupportedFormat";
        case ConversionError::CorruptFile: return "CorruptFile";
        case ConversionError::PasswordProtected: return "PasswordProtected";
        case ConversionError::ScannedPdfNoOcr: return "ScannedPdfNoOcr";
        case ConversionError::Timeout: return "Timeout";
        case ConversionError::Cancelled: return "Cancelled";
        default: return "Unknown";
    }
}

std::string error_msg(ConversionError e) {
    switch(e) {
        case ConversionError::ToolMissing: return "Required tool not installed.";
        case ConversionError::SourceNotFound: return "Source file not found.";
        case ConversionError::UnsupportedFormat: return "Format not supported.";
        case ConversionError::CorruptFile: return "File corrupted.";
        case ConversionError::PasswordProtected: return "PDF encrypted. Provide password or skip.";
        case ConversionError::ScannedPdfNoOcr: return "Scanned PDF. OCR not available.";
        case ConversionError::Timeout: return "Conversion timed out.";
        case ConversionError::Cancelled: return "Cancelled.";
        default: return "Unknown error.";
    }
}

enum class ConversionDirection {
    MD_to_DOCX, DOCX_to_MD, MD_to_HTML, HTML_to_MD, MD_to_PDF, PDF_to_MD, Unknown
};

struct StructBlock {
    enum Type { Heading, Paragraph, ListItem, CodeBlock, TableCell, Blockquote };
    Type type{Paragraph};
    int level{0}, sourceLine{-1}, sourcePage{-1};
    std::string text;
};

struct SourceSpan {
    int page{-1}, lineStart{-1}, charStart{-1};
    std::string anchor;
};

struct TextExtractionRequest {
    std::string source_path, source_format;
    bool prefer_structure{true};
};

struct TextExtractionResult {
    std::string plain_text, markdown_text, error;
    std::vector<StructBlock> blocks;
    std::vector<SourceSpan> spans;
    ConversionError error_code{ConversionError::None};
    bool ok{false};
};

struct ConversionCapabilities {
    bool pandoc_ok{false}, tectonic_ok{false}, poppler_ok{false};
    std::string pandoc_version, tectonic_version, poppler_version;
    std::string pandoc_path, tectonic_path, poppler_path;
    bool can_extract(Format f) const {
        if(f==Format::Markdown) return true;
        if(f==Format::DOCX||f==Format::HTML) return pandoc_ok;
        if(f==Format::PDF) return poppler_ok;
        return false;
    }
};

struct TaskInput {
    std::string source_path;
    std::vector<uint8_t> document_snapshot;
    bool is_memory_source{false};
    Format source_format{Format::Unknown}, target_format{Format::Unknown};
    std::string output_path, resource_path;
    bool overwrite_existing{false};
};

struct TaskOutput {
    TaskStatus status{TaskStatus::Pending};
    std::vector<std::string> logs;
    std::optional<std::string> product_path, error_message;
    ConversionError error_code{ConversionError::None};
    int exit_code{-1};
    std::chrono::milliseconds duration{0};
};

struct ToolStatus {
    std::string name, version, path;
    bool available{false};
    std::optional<std::string> error_message;
};

struct ServiceStats {
    size_t total_tasks{0}, completed_tasks{0}, failed_tasks{0}, pending_tasks{0}, running_tasks{0};
};

Format fmt_from_str(const std::string& s) {
    std::string l=s; std::transform(l.begin(),l.end(),l.begin(),::tolower);
    if(l=="markdown"||l=="md") return Format::Markdown;
    if(l=="docx") return Format::DOCX;
    if(l=="html"||l=="htm") return Format::HTML;
    if(l=="pdf") return Format::PDF;
    return Format::Unknown;
}

std::string fmt_str(Format f) {
    switch(f){case Format::Markdown:return "Markdown";case Format::DOCX:return "DOCX";case Format::HTML:return "HTML";case Format::PDF:return "PDF";default:return "Unknown";}
}

ConversionDirection direction(Format s, Format t) {
    if(s==Format::Markdown&&t==Format::DOCX) return ConversionDirection::MD_to_DOCX;
    if(s==Format::DOCX&&t==Format::Markdown) return ConversionDirection::DOCX_to_MD;
    if(s==Format::Markdown&&t==Format::HTML) return ConversionDirection::MD_to_HTML;
    if(s==Format::HTML&&t==Format::Markdown) return ConversionDirection::HTML_to_MD;
    if(s==Format::Markdown&&t==Format::PDF) return ConversionDirection::MD_to_PDF;
    if(s==Format::PDF&&t==Format::Markdown) return ConversionDirection::PDF_to_MD;
    return ConversionDirection::Unknown;
}

std::string bundled_dir() { return "./bundled_tools"; }

std::optional<std::string> find_exe(const std::string& name) {
    auto try_p = [](const std::string& p){FILE* f=fopen(p.c_str(),"r");if(f){fclose(f);return true;}return false;};
    std::string bp=bundled_dir()+"/"+name;
    if(try_p(bp)) return bp;
    const char* pe=getenv("PATH");
    if(pe){
        std::string ps(pe); size_t s=0,c;
        while((c=ps.find(':',s))!=std::string::npos){
            std::string fp=ps.substr(s,c-s)+"/"+name;
            if(try_p(fp)) return fp;
            s=c+1;
        }
    }
    return std::nullopt;
}

struct RunResult { int exit_code{0}; std::string out, err; bool timed_out{false}; int64_t duration_ms{0}; };

RunResult run_proc(const std::string& exe, const std::vector<std::string>& args, int timeout_ms=300000) {
    RunResult res;
    auto start=std::chrono::steady_clock::now();
#ifdef _WIN32
    std::string cmd=exe;
    for(auto& a:args) cmd+=" \""+a+"\"";
    SECURITY_ATTRIBUTES sa={sizeof(sa),nullptr,TRUE};
    HANDLE sr,sw,er,ew;
    CreatePipe(&sr,&sw,&sa,0); CreatePipe(&er,&ew,&sa,0);
    STARTUPINFOA si={sizeof(si)};
    si.dwFlags=STARTF_USESTDHANDLES; si.hStdOutput=sw; si.hStdError=ew;
    PROCESS_INFORMATION pi={0};
    if(!CreateProcessA(nullptr,&cmd[0],nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)){
        res.exit_code=-1; res.err="Failed"; return res;
    }
    CloseHandle(sw); CloseHandle(ew);
    char buf[4096]; DWORD br;
    while(ReadFile(sr,buf,sizeof(buf),&br,nullptr)&&br>0) res.out.append(buf,br);
    while(ReadFile(er,buf,sizeof(buf),&br,nullptr)&&br>0) res.err.append(buf,br);
    WaitForSingleObject(pi.hProcess,timeout_ms);
    GetExitCodeProcess(pi.hProcess,(DWORD*)&res.exit_code);
    CloseHandle(pi.hProcess); CloseHandle(sr); CloseHandle(er);
#else
    int sp[2],ep[2]; pipe(sp); pipe(ep);
    pid_t pid=fork();
    if(pid==0){
        close(sp[0]); close(ep[0]);
        dup2(sp[1],STDOUT_FILENO); dup2(ep[1],STDERR_FILENO);
        close(sp[1]); close(ep[1]);
        std::vector<char*> av;
        av.push_back(const_cast<char*>(exe.c_str()));
        for(auto& a:args) av.push_back(const_cast<char*>(a.c_str()));
        av.push_back(nullptr);
        execvp(exe.c_str(),av.data());
        _exit(127);
    }
    close(sp[1]); close(ep[1]);
    char buf[4096]; ssize_t br;
    while((br=read(sp[0],buf,sizeof(buf)))>0) res.out.append(buf,br);
    close(sp[0]);
    while((br=read(ep[0],buf,sizeof(buf)))>0) res.err.append(buf,br);
    close(ep[0]);
    int st; auto end=start+std::chrono::milliseconds(timeout_ms);
    while(std::chrono::steady_clock::now()<end){
        if(waitpid(pid,&st,WNOHANG)==pid){res.exit_code=WIFEXITED(st)?WEXITSTATUS(st):-1;break;}
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if(res.exit_code==-1){kill(pid,SIGKILL);waitpid(pid,&st,0);res.timed_out=true;}
#endif
    res.duration_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
    return res;
}

std::string create_temp_dir() {
#ifdef _WIN32
    char t[MAX_PATH]; GetTempPathA(MAX_PATH,t); strcat(t,"/cs_tmp"); mkdir(t); return t;
#else
    std::string t="/tmp/cs_XXXXXX"; std::vector<char> v(t.begin(),t.end()); v.push_back('\0');
    return mkdtemp(v.data())?v.data():"";
#endif
}

void cleanup_dir(const std::string& d) {
#ifdef _WIN32
    system(("rmdir /s /q \""+d+"\"").c_str());
#else
    system(("rm -rf \""+d+"\"").c_str());
#endif
}

bool file_exists(const std::string& p) { std::ifstream f(p.c_str()); return f.good(); }

std::string read_file(const std::string& p) {
    std::ifstream f(p,std::ios::binary); if(!f.good()) return "";
    std::ostringstream s; s<<f.rdbuf(); return s.str();
}

bool is_password_protected(const std::string& err) {
    std::string l=err; std::transform(l.begin(),l.end(),l.begin(),::tolower);
    return l.find("password")!=std::string::npos||l.find("encrypted")!=std::string::npos;
}

std::vector<StructBlock> parse_blocks(const std::string& md, int page=-1) {
    std::vector<StructBlock> blocks;
    std::istringstream stream(md);
    std::string line, para;
    int ln=0, para_ln=-1;
    bool in_code=false;
    StructBlock code;

    auto flush=[&](){
        if(!para.empty()){
            StructBlock b; b.type=StructBlock::Paragraph; b.text=para;
            b.sourceLine=para_ln; b.sourcePage=page; blocks.push_back(b);
            para.clear(); para_ln=-1;
        }
    };

    while(std::getline(stream,line)){
        ln++;
        std::string t=line;
        size_t fs=t.find_first_not_of(" \t\r\n");
        if(fs!=std::string::npos) t=t.substr(fs); else t="";
        if(t.empty()&&!in_code){flush();continue;}
        if(t.find("```")==0){
            if(in_code){code.text.pop_back();blocks.push_back(code);in_code=false;code=StructBlock();}
            else{flush();in_code=true;code.type=StructBlock::CodeBlock;code.sourceLine=ln;code.sourcePage=page;code.text="";}
            continue;
        }
        if(in_code){code.text+=line+"\n";continue;}
        if(t[0]=='#'){
            flush();
            int lvl=0; while(lvl<(int)t.size()&&t[lvl]=='#') lvl++;
            StructBlock b; b.type=StructBlock::Heading; b.level=std::min(lvl,6);
            std::string txt=t.substr(lvl); size_t s=txt.find_first_not_of(" \t");
            b.text=(s!=std::string::npos)?txt.substr(s):txt;
            b.sourceLine=ln; b.sourcePage=page; blocks.push_back(b); continue;
        }
        if(t[0]=='>'||t.find("&gt;")==0){
            flush();
            StructBlock b; b.type=StructBlock::Blockquote;
            std::string txt=(t[0]=='>')?t.substr(1):t; size_t s=txt.find_first_not_of(" \t");
            b.text=(s!=std::string::npos)?txt.substr(s):txt;
            b.sourceLine=ln; b.sourcePage=page; blocks.push_back(b); continue;
        }
        if(t[0]=='-'||t[0]=='*'||t[0]=='+'||(t.size()>2&&std::isdigit((unsigned char)t[0])&&t[1]=='.')){
            flush();
            StructBlock b; b.type=StructBlock::ListItem;
            size_t s=t.find_first_not_of("-*+0123456789. ");
            b.text=(s!=std::string::npos)?t.substr(s):t;
            b.sourceLine=ln; b.sourcePage=page; blocks.push_back(b); continue;
        }
        if(t.find('|')!=std::string::npos&&t.back()=='|'){
            flush();
            StructBlock b; b.type=StructBlock::TableCell; b.text=t;
            b.sourceLine=ln; b.sourcePage=page; blocks.push_back(b); continue;
        }
        if(para.empty()) para_ln=ln;
        para+=(para.empty()?"":" ")+t;
    }
    flush();
    return blocks;
}

std::vector<SourceSpan> build_spans(const std::vector<StructBlock>& blocks) {
    std::vector<SourceSpan> spans;
    for(auto& b:blocks){
        SourceSpan sp; sp.page=b.sourcePage; sp.lineStart=b.sourceLine;
        if(b.type==StructBlock::Heading){
            std::string slug;
            for(char c:b.text){
                if(std::isalnum((unsigned char)c)) slug+=(char)std::tolower((unsigned char)c);
                else if(c==' ') slug+='-';
            }
            sp.anchor=slug;
        }
        spans.push_back(sp);
    }
    return spans;
}

class ToolRunner {
public:
    struct RunOptions { int timeout_ms{300000}; std::string working_dir; };
    static std::optional<std::string> find_executable(const std::string& name, const std::vector<std::string>& paths={}) {
        for(auto& p:paths){std::string fp=p+"/"+name;FILE* f=fopen(fp.c_str(),"r");if(f){fclose(f);return fp;}}
        return find_exe(name);
    }
    static std::string get_bundled_tools_dir() { return bundled_dir(); }
};

class PandocConverter {
    mutable std::optional<bool> avail_;
public:
    bool is_available() const {
        if(avail_.has_value()) return avail_.value();
        auto p=ToolRunner::find_executable("pandoc",{"./bundled_tools/pandoc"});
        avail_=(p&&file_exists(*p)); return avail_.value();
    }
    static std::string get_executable_path() {
        auto r=ToolRunner::find_executable("pandoc",{"./bundled_tools/pandoc"});
        return r.value_or("");
    }
    RunResult md_to_docx(const std::string& in, const std::string& out, const std::string& res="") {
        std::vector<std::string> a={in,"-o",out,"--standalone"};
        if(!res.empty()){a.push_back("--resource-path");a.push_back(res);}
        auto p=get_executable_path(); return p.empty()?RunResult{-1,"","pandoc not found",false,0}:run_proc(p,a);
    }
    RunResult docx_to_md(const std::string& in, const std::string& out) {
        auto p=get_executable_path(); return p.empty()?RunResult{-1,"","pandoc not found",false,0}:run_proc(p,{in,"-o",out,"--to","markdown"});
    }
    RunResult md_to_html(const std::string& in, const std::string& out, const std::string& res="") {
        std::vector<std::string> a={in,"-o",out,"--standalone","--self-contained"};
        if(!res.empty()){a.push_back("--resource-path");a.push_back(res);}
        auto p=get_executable_path(); return p.empty()?RunResult{-1,"","pandoc not found",false,0}:run_proc(p,a);
    }
    RunResult html_to_md(const std::string& in, const std::string& out) {
        auto p=get_executable_path(); return p.empty()?RunResult{-1,"","pandoc not found",false,0}:run_proc(p,{in,"-o",out,"--from","html","--to","markdown"});
    }
};

class TectonicConverter {
    mutable std::optional<bool> avail_;
public:
    bool is_available() const {
        if(avail_.has_value()) return avail_.value();
        auto p=ToolRunner::find_executable("tectonic",{"./bundled_tools/tectonic"});
        avail_=(p&&file_exists(*p)); return avail_.value();
    }
    static std::string get_executable_path() {
        auto r=ToolRunner::find_executable("tectonic",{"./bundled_tools/tectonic"});
        return r.value_or("");
    }
    RunResult md_to_pdf(const std::string& in, const std::string& out, const std::string& res="") {
        std::string dir=out; size_t ls=out.find_last_of("/\\");
        if(ls!=std::string::npos) dir=out.substr(0,ls); else dir=".";
        auto p=get_executable_path(); return p.empty()?RunResult{-1,"","tectonic not found",false,0}:run_proc(p,{in,"--outdir",dir});
    }
};

class PopplerExtractor {
    mutable std::optional<bool> avail_;
public:
    bool is_available() const {
        if(avail_.has_value()) return avail_.value();
        auto p=ToolRunner::find_executable("pdftotext",{"./bundled_tools/poppler"});
        avail_=(p&&file_exists(*p)); return avail_.value();
    }
    static std::string get_executable_path() {
        auto r=ToolRunner::find_executable("pdftotext",{"./bundled_tools/poppler"});
        return r.value_or("");
    }
    RunResult pdf_to_md(const std::string& in, const std::string& out) {
        auto p=get_executable_path(); return p.empty()?RunResult{-1,"","pdftotext not found",false,0}:run_proc(p,{"-layout",in,out});
    }
    RunResult pdf_to_stdout(const std::string& in) {
        auto p=get_executable_path(); return p.empty()?RunResult{-1,"","pdftotext not found",false,0}:run_proc(p,{"-layout",in,"-"});
    }
    bool is_scanned_pdf(const std::string& path) const {
        auto p=get_executable_path(); if(p.empty()) return false;
        RunResult r=run_proc(p,{"-layout",path,"-"});
        if(r.exit_code!=0) return false;
        size_t wc=0; bool iw=false;
        for(char c:r.out){if(std::isalnum((unsigned char)c)){if(!iw){iw=true;wc++;}}else iw=false;}
        return wc<50;
    }
};

class Diagnostics {
public:
    static constexpr const char* SCANNED_PDF_WARNING="PDF is scanned. OCR not available.";
    std::vector<ToolStatus> check_all_tools() const {
        return {check_tool("pandoc"),check_tool("tectonic"),check_tool("pdftotext")};
    }
    ToolStatus check_tool(const std::string& name) const {
        ToolStatus s; s.name=name;
        auto p=ToolRunner::find_executable(name,{"./bundled_tools"});
        if(p){s.available=true;s.path=*p;s.version="unknown";}
        else{s.available=false;s.error_message="Not found";}
        return s;
    }
    ConversionCapabilities get_capabilities() const {
        ConversionCapabilities cap;
        PandocConverter pandoc; TectonicConverter tectonic; PopplerExtractor poppler;
        cap.pandoc_ok=pandoc.is_available(); cap.pandoc_path=PandocConverter::get_executable_path();
        cap.tectonic_ok=tectonic.is_available(); cap.tectonic_path=TectonicConverter::get_executable_path();
        cap.poppler_ok=poppler.is_available(); cap.poppler_path=PopplerExtractor::get_executable_path();
        return cap;
    }
};

class ResourceManager {
public:
    static std::string create_temp_dir() { return ::ConversionService::create_temp_dir(); }
    void cleanup_temp_dir(const std::string& d) { ::ConversionService::cleanup_dir(d); }
    bool confirm_overwrite(const std::string& p) const { return file_exists(p); }
    bool copy_file(const std::string& from, const std::string& to) const {
        std::ifstream s(from,std::ios::binary); std::ofstream d(to,std::ios::binary);
        if(!s||!d) return false; d<<s.rdbuf(); return d.good();
    }
    std::string resolve_relative_path(const std::string& base, const std::string& rel) const {
        if(rel.empty()) return base; if(rel[0]=='/') return rel;
        size_t ls=base.find_last_of("/\\");
        if(ls!=std::string::npos) return base.substr(0,ls)+"/"+rel;
        return rel;
    }
};

class TaskQueue {
    struct Entry {
        TaskHandle handle; TaskInput input; TaskOutput output;
        std::function<void(TaskHandle,const TaskOutput&)> cb;
        bool cancelled{false};
        std::chrono::steady_clock::time_point created_at;
    };
    std::queue<std::shared_ptr<Entry>> pq_;
    std::map<TaskHandle,std::shared_ptr<Entry>> rt_,ct_;
    mutable std::mutex mu_; std::condition_variable cv_;
    std::atomic<bool> stop_{false}; std::atomic<TaskHandle> nh_{1};
    std::vector<std::thread> wk_;
public:
    TaskQueue()=default;
    ~TaskQueue(){stop();}
    TaskHandle enqueue(const TaskInput& in, std::function<void(TaskHandle,const TaskOutput&)> cb=nullptr) {
        auto e=std::make_shared<Entry>();
        e->handle=nh_++; e->input=in; e->cb=cb;
        e->output.status=TaskStatus::Pending;
        e->created_at=std::chrono::steady_clock::now();
        {std::lock_guard<std::mutex> l(mu_);pq_.push(e);}
        cv_.notify_one(); return e->handle;
    }
    bool cancel(TaskHandle h) {
        std::lock_guard<std::mutex> l(mu_);
        std::queue<std::shared_ptr<Entry>> nq;
        while(!pq_.empty()){
            auto e=pq_.front();pq_.pop();
            if(e->handle==h){
                e->cancelled=true;e->output.status=TaskStatus::Cancelled;
                e->output.error_code=ConversionError::Cancelled;
                e->output.error_message="Cancelled";
                ct_[h]=e; if(e->cb) e->cb(e->handle,e->output); return true;
            }
            nq.push(e);
        }
        pq_=nq;
        auto it=rt_.find(h);
        if(it!=rt_.end()){it->second->cancelled=true;return true;}
        return false;
    }
    TaskOutput get_status(TaskHandle h) {
        std::lock_guard<std::mutex> l(mu_);
        auto it=ct_.find(h);if(it!=ct_.end())return it->second->output;
        it=rt_.find(h);if(it!=rt_.end())return it->second->output;
        std::queue<std::shared_ptr<Entry>> tmp;
        while(!pq_.empty()){
            auto e=pq_.front();pq_.pop();
            if(e->handle==h){auto o=e->output;tmp.push(e);pq_=tmp;return o;}
            tmp.push(e);
        }
        pq_=tmp; return {};
    }
    void start(size_t n=2) {
        for(size_t i=0;i<n;++i) wk_.emplace_back([this]{
            while(!stop_.load()){
                std::shared_ptr<Entry> e;
                {std::unique_lock<std::mutex> l(mu_);
                 cv_.wait(l,[&]{return stop_.load()||!pq_.empty();});
                 if(stop_.load()&&pq_.empty()) break;
                 if(!pq_.empty()){e=pq_.front();pq_.pop();rt_[e->handle]=e;}}
                if(e) process(e);
            }
        });
    }
    void stop() {
        stop_.store(true);cv_.notify_all();
        for(auto& w:wk_) if(w.joinable()) w.join();
        wk_.clear();
    }
    void wait_all() {
        std::unique_lock<std::mutex> l(mu_);
        cv_.wait(l,[&]{return pq_.empty()&&rt_.empty();});
    }
    bool empty() const {std::lock_guard<std::mutex> l(mu_);return pq_.empty()&&rt_.empty();}
    void clear() {std::lock_guard<std::mutex> l(mu_);while(!pq_.empty())pq_.pop();}
    size_t pending_count() const {std::lock_guard<std::mutex> l(mu_);return pq_.size()+rt_.size();}
    size_t running_count() const {std::lock_guard<std::mutex> l(mu_);return rt_.size();}
private:
    void process(std::shared_ptr<Entry> e) {
        auto t0=std::chrono::steady_clock::now();
        e->output.status=TaskStatus::Running;
        e->output.logs.push_back("Start: "+e->input.source_path+" -> "+fmt_str(e->input.target_format));
        RunResult r; bool ok=false;
        auto sf=e->input.source_format,tf=e->input.target_format;
        auto sp=e->input.source_path,op=e->input.output_path,rp=e->input.resource_path;
        PandocConverter pandoc; TectonicConverter tectonic; PopplerExtractor poppler;
        auto dir=direction(sf,tf);
        try {
            switch(dir) {
                case ConversionDirection::MD_to_DOCX:
                    if(!pandoc.is_available()){e->output.error_code=ConversionError::ToolMissing;e->output.error_message="Pandoc missing";break;}
                    r=pandoc.md_to_docx(sp,op,rp); ok=(r.exit_code==0); break;
                case ConversionDirection::DOCX_to_MD:
                    if(!pandoc.is_available()){e->output.error_code=ConversionError::ToolMissing;e->output.error_message="Pandoc missing";break;}
                    r=pandoc.docx_to_md(sp,op); ok=(r.exit_code==0); break;
                case ConversionDirection::MD_to_HTML:
                    if(!pandoc.is_available()){e->output.error_code=ConversionError::ToolMissing;e->output.error_message="Pandoc missing";break;}
                    r=pandoc.md_to_html(sp,op,rp); ok=(r.exit_code==0); break;
                case ConversionDirection::HTML_to_MD:
                    if(!pandoc.is_available()){e->output.error_code=ConversionError::ToolMissing;e->output.error_message="Pandoc missing";break;}
                    r=pandoc.html_to_md(sp,op); ok=(r.exit_code==0); break;
                case ConversionDirection::MD_to_PDF:
                    if(!tectonic.is_available()){e->output.error_code=ConversionError::ToolMissing;e->output.error_message="Tectonic missing";break;}
                    r=tectonic.md_to_pdf(sp,op,rp); ok=(r.exit_code==0); break;
                case ConversionDirection::PDF_to_MD:
                    if(!poppler.is_available()){e->output.error_code=ConversionError::ToolMissing;e->output.error_message="Poppler missing";break;}
                    if(poppler.is_scanned_pdf(sp)){e->output.error_code=ConversionError::ScannedPdfNoOcr;e->output.error_message=error_msg(ConversionError::ScannedPdfNoOcr);}
                    r=poppler.pdf_to_md(sp,op); ok=(r.exit_code==0); break;
                default:
                    e->output.error_code=ConversionError::UnsupportedFormat;e->output.error_message="Unsupported";break;
            }
            if(e->output.error_code!=ConversionError::None&&e->output.error_code!=ConversionError::ScannedPdfNoOcr){}
            else{
                e->output.exit_code=r.exit_code;
                if(r.timed_out){e->output.error_code=ConversionError::Timeout;e->output.error_message=error_msg(ConversionError::Timeout);ok=false;}
            }
        } catch(const std::exception& ex) {
            e->output.error_code=ConversionError::CorruptFile;e->output.error_message=std::string("Exception: ")+ex.what();
        }
        e->output.duration=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-t0);
        if(e->cancelled){e->output.status=TaskStatus::Cancelled;e->output.error_code=ConversionError::Cancelled;}
        else if(ok){e->output.status=TaskStatus::Completed;e->output.product_path=op;e->output.logs.push_back("Done "+std::to_string(e->output.duration.count())+"ms");}
        else{e->output.status=TaskStatus::Failed;if(!e->output.error_message)e->output.error_message="Failed";if(e->output.error_code==ConversionError::None)e->output.error_code=ConversionError::Unknown;}
        {std::lock_guard<std::mutex> l(mu_);rt_.erase(e->handle);ct_[e->handle]=e;}
        if(e->cb) e->cb(e->handle,e->output);
    }
};

class ConversionEngine {
    std::shared_ptr<TaskQueue> q_;
    mutable std::shared_mutex mu_;
    Diagnostics diag_; ResourceManager rm_;
    mutable std::atomic<size_t> total_{0},done_{0},fail_{0};
public:
    ConversionEngine():q_(std::make_shared<TaskQueue>()){q_->start(2);}
    ~ConversionEngine(){q_->stop();}
    TaskHandle convert(const TaskInput& in){
        std::unique_lock<std::shared_mutex> l(mu_);total_++;
        return q_->enqueue(in,[this](TaskHandle,const TaskOutput& o){
            if(o.status==TaskStatus::Completed)done_++;else if(o.status==TaskStatus::Failed)fail_++;
        });
    }
    std::vector<TaskHandle> convert_batch(const std::vector<TaskInput>& inputs){
        std::unique_lock<std::shared_mutex> l(mu_);
        std::vector<TaskHandle> h; h.reserve(inputs.size());
        for(auto& in:inputs){total_++;h.push_back(q_->enqueue(in,[this](TaskHandle,const TaskOutput& o){
            if(o.status==TaskStatus::Completed)done_++;else if(o.status==TaskStatus::Failed)fail_++;
        }));}
        return h;
    }
    TaskOutput get_status(TaskHandle h){std::shared_lock<std::shared_mutex> l(mu_);return q_->get_status(h);}
    bool cancel(TaskHandle h){std::shared_lock<std::shared_mutex> l(mu_);return q_->cancel(h);}
    void cancel_all(){std::shared_lock<std::shared_mutex> l(mu_);q_->clear();}
    void wait(TaskHandle h){
        while(true){auto s=get_status(h);
            if(s.status==TaskStatus::Completed||s.status==TaskStatus::Failed||s.status==TaskStatus::Cancelled)break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    void wait_all(){std::shared_lock<std::shared_mutex> l(mu_);q_->wait_all();}
    ServiceStats get_stats() const {
        ServiceStats s;s.total_tasks=total_.load();s.completed_tasks=done_.load();s.failed_tasks=fail_.load();
        s.pending_tasks=q_->pending_count();s.running_tasks=q_->running_count();return s;
    }
    bool is_tool_available(const std::string& n) const {
        if(n=="pandoc")return PandocConverter().is_available();
        if(n=="tectonic")return TectonicConverter().is_available();
        if(n=="pdftotext")return PopplerExtractor().is_available();
        return false;
    }
    std::vector<ConversionDirection> get_supported_directions() const {
        return {ConversionDirection::MD_to_DOCX,ConversionDirection::DOCX_to_MD,ConversionDirection::MD_to_HTML,
                ConversionDirection::HTML_to_MD,ConversionDirection::MD_to_PDF,ConversionDirection::PDF_to_MD};
    }
    bool can_convert(Format s,Format t) const {return direction(s,t)!=ConversionDirection::Unknown;}
    ConversionCapabilities capabilities() const {return diag_.get_capabilities();}

    TextExtractionResult extractText(const TextExtractionRequest& req) {
        TextExtractionResult result;
        if(!file_exists(req.source_path)){result.error="File not found";result.error_code=ConversionError::SourceNotFound;return result;}
        Format src_fmt=Format::Unknown;
        if(!req.source_format.empty()) src_fmt=fmt_from_str(req.source_format);
        if(src_fmt==Format::Unknown){size_t dot=req.source_path.find_last_of('.');if(dot!=std::string::npos)src_fmt=fmt_from_str(req.source_path.substr(dot+1));}
        if(src_fmt==Format::Unknown){result.error="Unknown format";result.error_code=ConversionError::UnsupportedFormat;return result;}
        if(src_fmt==Format::Markdown){
            std::string c=read_file(req.source_path);result.markdown_text=c;result.plain_text=c;
            if(req.prefer_structure){result.blocks=parse_blocks(c);result.spans=build_spans(result.blocks);}
            result.ok=true;return result;
        }
        auto caps=capabilities();
        if(!caps.can_extract(src_fmt)){result.error="Tool missing";result.error_code=ConversionError::ToolMissing;return result;}
        std::string tmp_dir=create_temp_dir();
        if(tmp_dir.empty()){result.error="Temp dir failed";result.error_code=ConversionError::Unknown;return result;}
        std::string tmp_out=tmp_dir+"/extracted.md";
        bool need_cleanup=true;
        auto cleanup=[&](){if(need_cleanup)cleanup_dir(tmp_dir);};
        if(src_fmt==Format::DOCX){
            PandocConverter p;if(!p.is_available()){cleanup();result.error="Pandoc missing";result.error_code=ConversionError::ToolMissing;return result;}
            auto r=p.docx_to_md(req.source_path,tmp_out);
            if(r.exit_code!=0){cleanup();if(is_password_protected(r.err)){result.error=error_msg(ConversionError::PasswordProtected);result.error_code=ConversionError::PasswordProtected;}else{result.error="DOCX failed";result.error_code=ConversionError::CorruptFile;}return result;}
        } else if(src_fmt==Format::HTML){
            PandocConverter p;if(!p.is_available()){cleanup();result.error="Pandoc missing";result.error_code=ConversionError::ToolMissing;return result;}
            auto r=p.html_to_md(req.source_path,tmp_out);
            if(r.exit_code!=0){cleanup();result.error="HTML failed";result.error_code=ConversionError::CorruptFile;return result;}
        } else if(src_fmt==Format::PDF){
            PopplerExtractor p;if(!p.is_available()){cleanup();result.error="Poppler missing";result.error_code=ConversionError::ToolMissing;return result;}
            auto test_r=p.pdf_to_stdout(req.source_path);
            if(test_r.exit_code!=0){cleanup();if(is_password_protected(test_r.err)){result.error=error_msg(ConversionError::PasswordProtected);result.error_code=ConversionError::PasswordProtected;}else{result.error="PDF failed";result.error_code=ConversionError::CorruptFile;}return result;}
            if(p.is_scanned_pdf(req.source_path)){cleanup();result.error=error_msg(ConversionError::ScannedPdfNoOcr);result.error_code=ConversionError::ScannedPdfNoOcr;return result;}
            std::string raw=test_r.out;
            std::string page_text;int page=1;
            std::vector<StructBlock> all_blocks;std::vector<SourceSpan> all_spans;
            std::istringstream ps(raw);std::string line;
            while(std::getline(ps,line)){
                if(line.find("\x0c")!=std::string::npos){
                    if(!page_text.empty()){auto pb=parse_blocks(page_text,page);auto ps2=build_spans(pb);
                        for(auto& b:pb)b.sourcePage=page;for(auto& s:ps2)s.page=page;
                        all_blocks.insert(all_blocks.end(),pb.begin(),pb.end());all_spans.insert(all_spans.end(),ps2.begin(),ps2.end());}
                    page++;page_text.clear();continue;
                }
                page_text+=line+"\n";
            }
            if(!page_text.empty()){auto pb=parse_blocks(page_text,page);auto ps2=build_spans(pb);
                for(auto& b:pb)b.sourcePage=page;for(auto& s:ps2)s.page=page;
                all_blocks.insert(all_blocks.end(),pb.begin(),pb.end());all_spans.insert(all_spans.end(),ps2.begin(),ps2.end());}
            result.markdown_text=raw;result.plain_text=raw;
            if(req.prefer_structure){result.blocks=all_blocks;result.spans=all_spans;}
            result.ok=true;need_cleanup=false;cleanup();return result;
        }
        std::string md=read_file(tmp_out);result.markdown_text=md;result.plain_text=md;
        if(req.prefer_structure){result.blocks=parse_blocks(md);result.spans=build_spans(result.blocks);}
        result.ok=true;need_cleanup=false;cleanup();return result;
    }
};

} // namespace ConversionService

using namespace ConversionService;

std::string status_str(TaskStatus s) {
    switch(s){case TaskStatus::Pending:return "Pending";case TaskStatus::Running:return "Running";case TaskStatus::Completed:return "Completed";case TaskStatus::Failed:return "Failed";case TaskStatus::Cancelled:return "Cancelled";}
    return "Unknown";
}

void print_task(const TaskOutput& o) {
    std::cout<<"\n--- Task Output ---\nStatus: "<<status_str(o.status)<<"\n";
    if(o.product_path) std::cout<<"Output: "<<*o.product_path<<"\n";
    if(o.error_message) std::cout<<"Error: "<<*o.error_message<<"\n";
    if(o.error_code!=ConversionError::None) std::cout<<"Code: "<<error_str(o.error_code)<<"\n";
    for(auto& l:o.logs) std::cout<<"  "<<l<<"\n";
    std::cout<<"Duration: "<<o.duration.count()<<"ms\n-------------------\n\n";
}

void print_extract(const TextExtractionResult& r) {
    std::cout<<"\n--- Extraction ---\nOK: "<<(r.ok?"Yes":"No")<<"\n";
    if(!r.error.empty()) std::cout<<"Error: "<<r.error<<"\n";
    if(r.error_code!=ConversionError::None) std::cout<<"Code: "<<error_str(r.error_code)<<"\n";
    std::cout<<"Blocks: "<<r.blocks.size()<<"\nSpans: "<<r.spans.size()<<"\n";
    if(!r.blocks.empty()){
        std::cout<<"\nStructure:\n";
        for(size_t i=0;i<std::min(r.blocks.size(),(size_t)20);++i){
            auto& b=r.blocks[i];std::string tn;
            switch(b.type){case StructBlock::Heading:tn="H"+std::to_string(b.level);break;case StructBlock::Paragraph:tn="P";break;
                case StructBlock::ListItem:tn="LI";break;case StructBlock::CodeBlock:tn="CODE";break;case StructBlock::TableCell:tn="TD";break;case StructBlock::Blockquote:tn="BQ";break;}
            std::cout<<"  ["<<tn<<"]";if(b.sourcePage>=0)std::cout<<" p"<<b.sourcePage;if(b.sourceLine>=0)std::cout<<" L"<<b.sourceLine;
            std::cout<<" "<<b.text.substr(0,60);if(b.text.size()>60)std::cout<<"...";std::cout<<"\n";
        }
        if(r.blocks.size()>20)std::cout<<"  ... +"<<(r.blocks.size()-20)<<" more\n";
    }
    std::cout<<"Text: "<<r.markdown_text.size()<<" chars\n------------------\n\n";
}

int main() {
    std::cout<<"\n========================================================\n"
            <<"         Conversion Service v2.0                        \n"
            <<"         Async, Cancellable Document Converter          \n"
            <<"         + Text Extraction for Knowledge Base           \n"
            <<"========================================================\n"
            <<"  Pandoc:  Markdown <-> DOCX, Markdown <-> HTML        \n"
            <<"  Tectonic: Markdown -> PDF                            \n"
            <<"  Poppler: PDF -> Markdown (text extraction)           \n"
            <<"  Extract: In-memory text + structure blocks           \n"
            <<"========================================================\n\n";

    Diagnostics diag;
    auto caps=diag.get_capabilities();
    std::cout<<"Tools: pandoc="<<(caps.pandoc_ok?"OK":"MISSING")<<" tectonic="<<(caps.tectonic_ok?"OK":"MISSING")<<" pdftotext="<<(caps.poppler_ok?"OK":"MISSING")<<"\n\n";

    auto svc=std::make_unique<ConversionEngine>();
    std::vector<TaskHandle> tasks;

    while(true){
        std::cout<<"1.Convert 2.Extract 3.Tools 4.Cancel 5.Stats 6.Caps 7.Help 0.Exit\n> ";
        std::string cmd;std::getline(std::cin,cmd);
        if(cmd.empty())std::getline(std::cin,cmd);
        if(cmd=="0"||cmd=="exit")break;
        else if(cmd=="1"){
            std::string sp,op,rp;
            std::cout<<"Source: ";std::getline(std::cin,sp);if(sp.empty()){std::cout<<"Required\n";continue;}
            std::cout<<"Src fmt: ";std::string sf;std::getline(std::cin,sf);
            auto src=fmt_from_str(sf);if(src==Format::Unknown){std::cout<<"Unknown\n";continue;}
            std::cout<<"Tgt fmt: ";std::string tf;std::getline(std::cin,tf);
            auto tgt=fmt_from_str(tf);if(tgt==Format::Unknown){std::cout<<"Unknown\n";continue;}
            if(!svc->can_convert(src,tgt)){std::cout<<"Not supported\n";continue;}
            std::cout<<"Output: ";std::getline(std::cin,op);if(op.empty()){std::cout<<"Required\n";continue;}
            std::cout<<"Resource: ";std::getline(std::cin,rp);
            ResourceManager rm;if(rm.confirm_overwrite(op)){std::cout<<"Overwrite? (y/n): ";std::string a;std::getline(std::cin,a);if(a!="y"&&a!="Y"){std::cout<<"Skip\n";continue;}}
            TaskInput in;in.source_path=sp;in.source_format=src;in.target_format=tgt;in.output_path=op;in.resource_path=rp;in.overwrite_existing=true;
            auto h=svc->convert(in);tasks.push_back(h);std::cout<<"Task "<<h<<"\n";
            svc->wait(h);print_task(svc->get_status(h));
        } else if(cmd=="2"){
            std::string sp;std::cout<<"Source: ";std::getline(std::cin,sp);if(sp.empty()){std::cout<<"Required\n";continue;}
            std::cout<<"Format: ";std::string sf;std::getline(std::cin,sf);
            TextExtractionRequest req;req.source_path=sp;req.source_format=sf;req.prefer_structure=true;
            auto r=svc->extractText(req);print_extract(r);
            if(r.ok&&r.markdown_text.size()<=500)std::cout<<r.markdown_text<<"\n\n";
        } else if(cmd=="3"){
            auto ts=diag.check_all_tools();
            for(auto& t:ts)std::cout<<"["<<t.name<<"] "<<(t.available?"OK":"MISSING")<<" "<<t.path<<"\n";
        } else if(cmd=="4"){
            if(tasks.empty()){std::cout<<"No tasks\n";continue;}
            std::cout<<"Tasks: ";for(auto h:tasks)std::cout<<h<<" ";std::cout<<"\nID: ";
            std::string id;std::getline(std::cin,id);
            try{auto h=std::stoull(id);if(svc->cancel(h))std::cout<<"Cancelled\n";else std::cout<<"Not found\n";}catch(...){std::cout<<"Invalid\n";}
        } else if(cmd=="5"){
            auto s=svc->get_stats();
            std::cout<<"Total: "<<s.total_tasks<<" Done: "<<s.completed_tasks<<" Fail: "<<s.failed_tasks<<" Pending: "<<s.pending_tasks<<" Running: "<<s.running_tasks<<"\n";
        } else if(cmd=="6"){
            auto c=svc->capabilities();
            std::cout<<"pandoc: "<<(c.pandoc_ok?"OK":"MISS")<<" tectonic: "<<(c.tectonic_ok?"OK":"MISS")<<" pdftotext: "<<(c.poppler_ok?"OK":"MISS")<<"\n";
            std::cout<<"Extract: MD=Yes DOCX="<<c.pandoc_ok<<" HTML="<<c.pandoc_ok<<" PDF="<<c.poppler_ok<<"\n";
        } else if(cmd=="7"){
            std::cout<<"MD<->DOCX, MD<->HTML, MD->PDF, PDF->MD\nExtract: text+blocks (no disk)\nBlocks: H/P/LI/CODE/TD/BQ\nSpans: page+line+anchor\nErrors: ScannedPdfNoOcr, PasswordProtected\n";
        }
    }
    return 0;
}
