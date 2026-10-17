#include <string>
#include <vector>
#include <fstream>
#include <deque>
#include <span>
#include <unordered_set>
#include <unordered_map>
#include <imageinfo.hpp>
#include <fmt/base.h>
#include <fmt/ostream.h>
#include <curl/curl.h>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>


// Implements 'defer'
template <class F>
struct Deferrer {
	F f;
	~Deferrer() {
		f();
	}
};
// This is a small hack to make the final syntax look nicer
// If you simply called the Deferrer constructor directly, you'd have to wrap it in parentheses:
//     defer( { cleanup(obj); } );
// With the tag + overload, it's just:
//     defer { cleanup(obj); };
// We use the rather rare ->* operator, just for shits and giggles
struct DeferrerSetupTag {};
template <class F>
Deferrer<F> operator->*(DeferrerSetupTag, F f) {
	return Deferrer(f);
}
#define DEFER_(LINE) zzz_defer_##LINE
#define DEFER(LINE) DEFER_(LINE)
#define defer auto DEFER(__LINE__) = DeferrerSetupTag{} ->* [&]()->void


struct CurlContext
{
	CURL* m_curl;
	std::string m_readbuf;

	static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
		std::string& readbuf = *(std::string*)userp;
		if (readbuf.size() > 2'000'000) {
			// We all have our limits...
			return 0;
		}
		readbuf.append((char*)contents, size * nmemb);
		return size * nmemb;
	}

	CurlContext() {
		CURLcode result{};

		result = curl_global_init(CURL_GLOBAL_ALL);
		if (result != CURLE_OK) {
			fmt::println(stderr, "Global libcurl initialization failed: {}", curl_easy_strerror(result));
			exit(1);
		}

		m_curl = curl_easy_init();
		if (!m_curl) {
			fmt::println("Failed to create curl handle");
			exit(1);
		}

		curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, &writeCallback);
		curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &m_readbuf);
		curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
		curl_easy_setopt(m_curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA | CURLSSLOPT_AUTO_CLIENT_CERT);
		curl_easy_setopt(m_curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
		curl_easy_setopt(m_curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2);
		curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_LIMIT, 100);
		curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_TIME, 10);
		//curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1);
	}

	~CurlContext() {
		if (m_curl) {
			curl_easy_cleanup(m_curl);
			m_curl = nullptr;
		}
		curl_global_cleanup();
	}

	CurlContext(const CurlContext&) = delete;

	std::span<char> fetch(const std::string& url) {
		CURLcode result{};

		m_readbuf.clear();

		fmt::println("Fetching: {}", url);
		curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());

		result = curl_easy_perform(m_curl);
		if (result != CURLE_OK) {
			fmt::println(stderr, "curl_easy_perform() failed: {}", curl_easy_strerror(result));
			return {};
		}

		return { m_readbuf.data(), m_readbuf.size() };
	}
};
CurlContext curl;


std::unordered_set<std::string> visited_urls{};


struct UrlProperty {
	std::string url;
};
using Graph = boost::adjacency_list<boost::hash_setS, boost::vecS, boost::directedS, UrlProperty>;
Graph graph{};

std::unordered_map<std::string, Graph::vertex_descriptor> url_verts{};


static
std::string urlToHostname(const std::string& url)
{
	CURLUcode rc{};
	CURLU* curlu = curl_url();
	rc = curl_url_set(curlu, CURLUPART_URL, url.c_str(), 0);
	if (rc) {
		fmt::println(stderr, "curl_url_set() failed: {}", curl_url_strerror(rc));
		return {};
	}
	defer { curl_url_cleanup(curlu); };

	char* hostname{};
	rc = curl_url_get(curlu, CURLUPART_HOST, &hostname, 0);
	if (rc) {
		fmt::println(stderr, "curl_url_get() failed: {}", curl_url_strerror(rc));
		return {};
	}
	defer { curl_free(hostname); };

	char* scheme{}; // or 'protocol'
	rc = curl_url_get(curlu, CURLUPART_SCHEME, &scheme, 0);
	if (rc) {

	}
	defer { curl_free(scheme); };

	return std::string(scheme).append("://").append(hostname);
}


static
bool checkIfImgIsButton(const std::string& url)
{
	auto span = curl.fetch(url);
	if (span.empty()) {
		fmt::println(stderr, "Skipping {} - could not fetch button", url);
		return false;
	}
	auto info = imageinfo::parse<imageinfo::RawDataReader>(imageinfo::RawData(span.data(), span.size()));
	auto dim = info.size();
	return (dim.width == 88 && dim.height == 31); // || (dim.width == 80 && dim.height == 15); 
}


static
std::string resolveRef(std::string_view ref, const std::string& parent_url)
{
	// Filter archived links to their base from
	if (ref.find("://") != ref.rfind("://")) {
		// Leave at "//host.tld/path..."
		ref = ref.substr(ref.rfind("://") + 1);
	}
	if (ref.contains("://"))
		return std::string(ref);
	if (ref.starts_with("//"))
		return std::string("https:").append(ref);
	if (ref.starts_with('/'))
		return urlToHostname(parent_url).append(ref);
	return urlToHostname(parent_url).append("/").append(ref);
}


// Input must be a full URL with scheme and hostname
static
void cleanURL(std::string& url)
{
	// Remove query and fragment
	url = std::string_view(url).substr(0, url.find_first_of("?#"));
	// Remove trailing slash
	if (url.ends_with('/'))
		url.pop_back();

	// Remove file, but not .html files
	// https://example.com/site/index.html
	// https://example.com/buttons/button.png

	size_t last_slash_idx = url.find_last_of('/');
	size_t scheme_second_slash_idx = url.find("://") + 2;
	if (scheme_second_slash_idx == last_slash_idx)
		return; // No file
	auto file = std::string_view(url).substr(last_slash_idx + 1);
	if (file.ends_with(".html") || file.ends_with(".htm"))
		return; // HTML file, OK
	url = std::string_view(url).substr(0, last_slash_idx);
	if (url.ends_with('/'))
		url.pop_back();
}


static
void traverseAndFindAllImgTags(xmlDoc* doc, const std::string& url, std::vector<std::string>& child_urls)
{
	xmlXPathContext* context = xmlXPathNewContext(doc);
	if (!context) {
		fmt::println(stderr, "Could not create XPath context");
		return;
	}
	defer { xmlXPathFreeContext(context); };

	xmlXPathObject* result = xmlXPathEvalExpression((const xmlChar*)"//img", context);
	if (!result) {
		fmt::println("Skipping {} - No <img> tags", url);
		return;
	}
	defer { xmlXPathFreeObject(result); };

	xmlNodeSet* nodeset = result->nodesetval;
	if (!nodeset) {
		fmt::println(stderr, "Skipping {} - No nodeset?", url);
		return;
	}

	for (int i = 0; i < nodeset->nodeNr; i++) {
		xmlNode* img_node = nodeset->nodeTab[i];

		const xmlChar* src_attr = xmlGetProp(img_node, (const xmlChar*)"src");
		if (!src_attr) {
			fmt::println("Skipping <img> tag with no src attribute");
			continue;
		}
		std::string img_url = resolveRef(std::string_view((const char*)src_attr), url);
		xmlFree((void*)src_attr);

		// Intentionally check everything else first before checking the size of the image,
		// so that we don't make unnecessary requests

		xmlNode* parent_node = img_node->parent;
		if (!parent_node || !parent_node->name || xmlStrcasecmp(parent_node->name, (const xmlChar*)"a") != 0) {
			fmt::println("Skipping <img> tag that is not a child of an <a> tag");
			continue;
		}

		const xmlChar* href_attr = xmlGetProp(parent_node, (const xmlChar*)"href");
		if (!href_attr) {
			fmt::println("Skipping <img> whose parent <a> tag has not href attribute");
			continue;
		}
		std::string child_url = resolveRef(std::string_view((const char*)href_attr), url);
		xmlFree((void*)href_attr);
		cleanURL(child_url);

		if (!checkIfImgIsButton(img_url)) {
			fmt::println("Skipping <img> which doesn't have the dimensions of a button");
			continue;
		}

		if (!url_verts.contains(child_url)) {
			fmt::println("Adding parent host to visit: {}", child_url);
			child_urls.push_back(child_url);
			auto vertex = boost::add_vertex(UrlProperty(child_url), graph);
			url_verts.insert({ child_url, vertex });
			boost::add_edge(url_verts.at(url), vertex, graph);
		}
		else {
			fmt::println("Logging duplicate: {}", child_url);
			boost::add_edge(url_verts.at(url), url_verts.at(child_url), graph);
		}
	}
}


static
std::vector<std::string> getRelatedUrls(const std::string& url)
{
	// There shouldn't be duplicates here, but check just in case
	if (visited_urls.contains(url))
		return {};
	visited_urls.insert(url);

	auto span = curl.fetch(url);
	if (span.empty()) {
		fmt::println(stderr, "Skipping {} - could not fetch", url);
		return {};
	}

	xmlDoc* doc = htmlReadMemory(span.data(), span.size(), url.c_str(), nullptr, HTML_PARSE_NOWARNING | HTML_PARSE_HTML5 | XML_PARSE_HUGE | XML_PARSE_NOERROR);
	if (!doc) {
		fmt::println(stderr, "Skipping {} - could not parse", url);
		return {};
	}
	defer { xmlFreeDoc(doc); };

	std::vector<std::string> child_urls = {};
	traverseAndFindAllImgTags(doc, url, child_urls);

	return child_urls;
}


static
void iterHTMLpages(const std::string& base_url, int max_depth)
{
	int depth = 0;
	std::deque<std::string> urls = {base_url};
	while (!urls.empty() && depth++ < max_depth) {
		std::string url = std::move(urls.front());
		urls.pop_front();

		fmt::println("Visiting: {}", url);
		auto child_urls = getRelatedUrls(url);
		urls.append_range(std::move(child_urls));
	}
}


struct NameWriter {
	template <class Vertex>
	void operator()(std::ostream& out, const Vertex& v) const {
		out << "[label=\"" << graph[v].url << "\"]";
	}
};


int main(int argc, const char* argv[])
{
	std::string url = {};
	if (argc < 2) {
		//url = "https://www.unix.dog/~yosh"; // From dog run
		url = "https://beepi.ng/"; // From machine run, also archive links
		//url = "https://medicinebottle.neocities.org/"; // From neocities run
		//url = "https://arch.dog"; // Hidden JS-only buttons! (Not accounted for)
		//url = "https://www.thunderbird.net"; // Segfaults tidy-html5 :)
		//url = "https://lina.sh"; // Example of an infinite stream (User-agent based)
		//url = "https://minecraft.net"; // Example of an unresponsive page
		//url = "https://adryd.com/"; // Example of link URLs without a scheme (prefixed with only '//')
		//url = "https://cobaltmusic.fi";
		//url = "https://bun-e.fyi/";
		fmt::println("Using default url: {}", url);
	}
	else {
		url = argv[1];
	}

	//int max_depth = 2000;
	int max_depth = 1;
	if (argc < 3) {
		fmt::println("Using default depth: {}", max_depth);
	}
	else {
		max_depth = atoi(argv[2]);
	}

	if (url.ends_with('/'))
		url.pop_back();
	if (!url.contains("://"))
		url = "https://" + url;
	auto vert = boost::add_vertex(UrlProperty(url), graph);
	url_verts.insert({ url, vert });

	iterHTMLpages(url, max_depth);

	std::ofstream outf("out.dot");
	boost::write_graphviz(outf, graph, NameWriter{});

	return 0;
}