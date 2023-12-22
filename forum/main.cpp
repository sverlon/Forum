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

void send_http_response(int clientSocket, const char *response)
{
    // Send the HTTP response headers
    const char *headers = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n";
    send(clientSocket, headers, strlen(headers), 0);

    // Send the response body
    send(clientSocket, response, strlen(response), 0);
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

void inject_posts(std::string &html_content, const std::string &posts)
{
    size_t pos = html_content.find("</article>");
    if (pos != std::string::npos)
    {
        html_content.insert(pos, posts + "\n");
    }
}

void serve_client(int client_socket)
{
    // Receive data from the client
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead = recv(client_socket, buffer, sizeof(buffer), 0);

    std::string message(buffer, bytesRead);

    std::cout << "Got HTTP Request: " << message << std::endl;

    if (!message.size())
    {
        std::cout << "Exiting client session..." << std::endl;
        close(client_socket);
        return;
    }

    size_t start = message.find(" ") + 1;
    size_t end = message.find(" ", start);
    if (start == std::string::npos || end == std::string::npos)
        std::cerr << "Error: HTTP request mal formatted." << std::endl;

    std::string url = message.substr(start, end - start);
    std::cout << "Parsed URL: " << url << std::endl;

    if (url == "/")
    {
        // In this case we serve the homepage!
        // Need to get posts from database.
        int db_socket = connect_to_server("192.168.1.234", 8080);
        std::string req = "1";
        send(db_socket, req.data(), req.size(), 0);

        // Next, we will receive the blog posts.
        char buffer[BUFFER_SIZE];
        uint32_t num_blog_posts;
        recv(db_socket, &num_blog_posts, 4, MSG_WAITALL);
        num_blog_posts = ntohl(num_blog_posts);

        std::cout << "Receiving " << num_blog_posts << " blog posts..." << std::endl;

        std::string all_posts;

        for (size_t i = 0; i < num_blog_posts; i++)
        {
            recv(db_socket, buffer, BUFFER_SIZE, 0);
            std::string post(buffer);

            size_t split_pos = post.find("|");
            std::string title = post.substr(0, split_pos);
            std::string content = post.substr(split_pos + 1);

            std::string html_block = "<article><h2>";
            html_block += title;
            html_block += "</h2><p>";
            html_block += content;
            html_block += "</p></article>";
            all_posts += html_block;
        }

        std::cout << "Received." << std::endl;

        // Respond with the homepage.
        auto html_content = get_html_content("forum/homepage.html");

        // Inject style.css for this page.
        inject_css(html_content, "forum/style.css");

        // Inject the homepage javascript.
        inject_js(html_content, "forum/homepage.js");

        // Inject the blog posts.
        inject_posts(html_content, all_posts);

        const char *response_format = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %zu\r\n\r\n%s";
        char response_buffer[strlen(response_format) + html_content.size()];
        int response_length = snprintf(response_buffer, sizeof(response_buffer), response_format, html_content.size(), html_content.data());
        send(client_socket, response_buffer, response_length, 0);
    }
    else if (url.find("insert_post") != std::string::npos)
    {

        // Get the title.
        size_t title_start = url.find("=");
        size_t title_end = url.find("&content=");
        auto title = url.substr(title_start + 1, title_end);
        url = url.substr(title_end + 1);

        // Then, get content.
        size_t content_start = url.find("=");
        auto content = url.substr(content_start + 1);

        // Gotta set up a connection with
        // the database server.
        int db_socket = connect_to_server("192.168.1.234", 8080);

        std::string req = "0 " + title + "|" + content;
        send(db_socket, req.data(), req.size(), 0);

        // We wait for ack.
        bytesRead = recv(client_socket, buffer, sizeof(buffer), 0);
        std::string ack(buffer, bytesRead);

        std::cout << "Received back: " << ack << std::endl;

        if (ack == "OK")
        {
            // Send an HTTP response back to the client
            const char *headers = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n";
            send(client_socket, headers, strlen(headers), 0);
        }
        else if (ack == "FAIL")
        {
            const char *headers = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\n";
            send(client_socket, headers, strlen(headers), 0);
        }
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
