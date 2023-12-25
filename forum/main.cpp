#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include "networking.h"

std::string get_html_content(const std::string &filepath)
{
    std::ifstream html_file(filepath);

    if (!html_file.is_open())
        throw std::runtime_error("Error: could not open .html");

    std::stringstream buffer;
    buffer << html_file.rdbuf();

    html_file.close();

    return buffer.str();
}

void inject_css(std::string &html_content, const std::string &css_filename)
{
    std::ifstream css_file(css_filename);

    if (!css_file.is_open())
        throw std::runtime_error("Error: could not open " + css_filename);

    std::stringstream buffer;
    buffer << css_file.rdbuf();

    css_file.close();

    // Include CSS reference in the HTML content
    size_t pos = html_content.find("</head>");
    if (pos != std::string::npos)
    {
        html_content.insert(pos, "<style>" + buffer.str() + "</style>\n");
    }
}

void inject_js(std::string &html_content, const std::string &js_filename)
{
    std::ifstream js_file(js_filename);

    if (!js_file.is_open())
        throw std::runtime_error("Error: could not open " + js_filename);

    std::stringstream buffer;
    buffer << js_file.rdbuf();

    js_file.close();

    // Include JavaScript reference in the HTML content
    size_t pos = html_content.find("</body>");
    if (pos != std::string::npos)
    {
        html_content.insert(pos, "<script>" + buffer.str() + "</script>\n");
    }
}

std::string urlDecode(const std::string &input)
{
    std::string result;
    for (size_t i = 0; i < input.size(); ++i)
    {
        if (input[i] == '%' && i + 2 < input.size() && std::isxdigit(input[i + 1]) && std::isxdigit(input[i + 2]))
        {
            // Found a URL-encoded sequence
            int value;
            std::istringstream hexStream(input.substr(i + 1, 2));
            hexStream >> std::hex >> value;
            result += static_cast<char>(value);
            i += 2; // Skip the two hexadecimal digits
        }
        else
        {
            // Copy the character as is
            result += input[i];
        }
    }
    return result;
}

void inject_posts(std::string &html_content, const std::string &posts)
{
    // First, we want to replace any instance of "%20" with a space.
    std::string cleaned_posts = urlDecode(posts);

    size_t pos = html_content.find("</article>");
    if (pos != std::string::npos)
    {
        html_content.insert(pos + 12, cleaned_posts + "\n");
    }
}

bool insert_post(const std::string &title, const std::string &content)
{
    char ack_buffer[10] = {0};
    // Gotta set up a connection with
    // the database server.
    int db_socket = connect_to_server("192.168.1.234", 8080);

    std::string req = "0 " + title + "|" + content;
    send(db_socket, req.data(), req.size(), 0);

    // We wait for ack.
    int bytesRead = recv(db_socket, ack_buffer, 10, MSG_WAITALL);
    std::string ack(ack_buffer, bytesRead);

    std::cout << "Received back: " << ack << std::endl;

    if (ack.find("OK") != std::string::npos)
    {
        std::cout << "Insertion request succeeded." << std::endl;
        return true;
    }
    else if (ack.find("FAIL") != std::string::npos)
    {
        std::cout << "Insertion request failed." << std::endl;
        return false;
    }
}

std::string get_url_from_http_request(const std::string &http_request)
{
    size_t start = http_request.find(" ") + 1;
    size_t end = http_request.find(" ", start);
    if (start == std::string::npos || end == std::string::npos)
        std::cerr << "Error: HTTP request mal formatted." << std::endl;

    return http_request.substr(start, end - start);
}

std::string get_all_posts()
{
    // Connect to database.
    int db_socket = connect_to_server("192.168.1.234", 8080);
    std::string req = "1";
    send(db_socket, req.data(), req.size(), 0);

    // Next, we will receive the blog posts.
    uint32_t num_blog_posts;
    recv(db_socket, &num_blog_posts, 4, MSG_WAITALL);
    num_blog_posts = ntohl(num_blog_posts);

    std::cout << "Receiving " << num_blog_posts << " blog posts..." << std::endl;

    std::string all_posts;

    for (size_t i = 0; i < num_blog_posts; i++)
    {
        char entry_buffer[BUFFER_SIZE] = {0};
        recv(db_socket, entry_buffer, BUFFER_SIZE, 0);

        std::cout << entry_buffer << std::endl;

        std::string entry(entry_buffer);

        size_t split_pos = entry.find("|");
        std::string title = entry.substr(0, split_pos);
        std::string content = entry.substr(split_pos + 1);

        std::string html_block = "<article><h2>";
        html_block += title;
        html_block += "</h2><p>";
        html_block += content;
        html_block += "</p></article>";
        all_posts += html_block;

        char ack = 0;
        send(db_socket, &ack, 1, 0);
    }

    std::cout << "Received." << std::endl;

    close(db_socket);

    return all_posts;
}

std::string get_homepage()
{
    // Grab all posts.
    std::string all_posts = get_all_posts();

    // Respond with the homepage.
    auto html_content = get_html_content("forum/homepage.html");

    // Inject style.css for this page.
    inject_css(html_content, "forum/style.css");

    // Inject the homepage javascript.
    inject_js(html_content, "forum/homepage.js");

    // Inject the blog posts.
    inject_posts(html_content, all_posts);

    return html_content;
}

std::pair<std::string, std::string> parse_title_and_content(std::string &url)
{
    // Get the title.
    size_t title_start = url.find("=");
    size_t title_end = url.find("&");
    auto title = url.substr(title_start + 1, title_end - title_start - 1);
    url = url.substr(title_end + 1);

    // Then, get content.
    size_t content_start = url.find("=");
    auto content = url.substr(content_start + 1);

    return {title, content};
}

void send_ok_http_with_content(int client_socket, const std::string &html_content)
{
    std::cout << "Sending html content:" << std::endl;
    std::cout << html_content << std::endl;

    const char *response_format = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nAccess-Control-Allow-Origin: http://192.168.1.234\r\nContent-Length: %zu\r\n\r\n%s";
    char response_buffer[strlen(response_format) + html_content.size()];
    int response_length = snprintf(response_buffer, sizeof(response_buffer), response_format, html_content.size(), html_content.data());
    send(client_socket, response_buffer, response_length, 0);
}

void send_fail_http(int client_socket)
{
    const char *headers = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\n";
    send(client_socket, headers, strlen(headers), 0);
}

std::string retrieve_http_request(int client_socket)
{
    // Receive data from the client
    char http_request_buffer[BUFFER_SIZE] = {0};
    ssize_t bytesRead = recv(client_socket, http_request_buffer, sizeof(http_request_buffer), 0);
    return std::string(http_request_buffer, bytesRead);
}

void serve_client(int client_socket)
{
    auto http_request = retrieve_http_request(client_socket);

    std::cout << "Got HTTP Request: \n\n"
              << http_request << std::endl;

    // An empty http request means close session.
    if (!http_request.size())
    {
        std::cout << "Exiting client session..." << std::endl;
        close(client_socket);
        return;
    }

    // Get the url from the request.
    std::string url = get_url_from_http_request(http_request);
    std::cout << "Parsed URL: " << url << std::endl;

    // Decode url.
    if (url == "/")
    {
        std::string homepage = get_homepage();

        send_ok_http_with_content(client_socket, homepage);
    }
    else if (url.find("insert_post") != std::string::npos)
    {

        auto [title, content] = parse_title_and_content(url);

        std::cout << "Parsed title: `" << title;
        std::cout << "`, Content: `" << content << "`" << std::endl;

        if (insert_post(title, content))
        {
            // Respond with the homepage.
            auto homepage = get_homepage();

            send_ok_http_with_content(client_socket, homepage);
        }
        else
            send_fail_http(client_socket);
    }
    else
        std::cerr << "Error: not a valid route." << std::endl;

    // Close the client socket
    close(client_socket);
}

int main()
{
    // Create a server socket
    auto server_socket = create_socket(nullptr, 80);

    std::cout << "Server listening on port 80...\n";

    while (true)
    {
        // Accept a connection
        int client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket == -1)
        {
            std::cerr << "Error accepting connection" << std::endl;
            continue;
        }
        std::cout << "Serving new client with socket fd: " << client_socket << std::endl;

        std::thread client_process(serve_client, client_socket);
        client_process.detach();
    }

    // Close the server socket
    close(server_socket);

    return 0;
}
