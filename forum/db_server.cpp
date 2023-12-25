#include <iostream>
#include <string>
#include <sqlite3.h>
#include <ranges>
#include <vector>

#include "networking.h"

enum class Status
{
    OK,
    DATABASE_OPEN_ERROR,
    INSERT_ERROR,
};

enum class Command
{
    INSERT_POST,
    RETRIEVE_POSTS
};

Status createPost(sqlite3 *db, std::string &title, const std::string &content)
{
    std::string insertDataSQL = "INSERT INTO Posts (title, content) VALUES ('" +
                                title +
                                "', '" +
                                content +
                                "');";

    int rc = sqlite3_exec(db, insertDataSQL.data(), 0, 0, 0);

    if (rc != SQLITE_OK)
    {
        std::cerr << "Error inserting data: " << sqlite3_errmsg(db) << std::endl;
        return Status::INSERT_ERROR;
    }

    return Status::OK;
}

int countCallback(void *data, int argc, char **argv, char **azColName)
{
    int *count = reinterpret_cast<int *>(data);
    *count = atoi(argv[0]); // Assuming the count is in the first column
    return 0;               // Returning 0 indicates success
}

Status getPosts(sqlite3 *db, int client_socket)
{
    // SQLite query to get the count of posts
    const char *countQuery = "SELECT COUNT(*) FROM Posts;";

    // Execute the count query using sqlite3_exec
    int postCount = 0;
    int rc = sqlite3_exec(db, countQuery, countCallback, &postCount, nullptr);
    if (rc != SQLITE_OK)
    {
        std::cerr << "Error getting post count: " << sqlite3_errmsg(db) << std::endl;
        return Status::DATABASE_OPEN_ERROR;
    }
    else
    {
        std::cout << "Number of posts: " << postCount << std::endl;
    }
    postCount = htonl(postCount);

    send(client_socket, &postCount, sizeof(int), 0);

    // SQLite query
    const char *sql = "SELECT title, content FROM Posts;";

    // Prepare the SQL statement
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        std::cerr << "Cannot prepare statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return Status::DATABASE_OPEN_ERROR;
    }

    // Execute the query
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        // Process each row
        const char *title = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        std::cout << "title: " << title << std::endl;
        std::string res(title);
        res += "|";
        const char *content = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        std::cout << "content: " << content << std::endl;
        res += std::string(content);

        char ack;
        send(client_socket, res.data(), res.size(), 0);
        recv(client_socket, &ack, 1, MSG_WAITALL);
    }

    // Finalize the statement and close the database
    sqlite3_finalize(stmt);
    return Status::OK;
}

int main()
{
    sqlite3 *db;
    int rc = sqlite3_open("forum/forum.db", &db);

    if (rc)
    {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return rc;
    }

    const char *createPostsSQL = "CREATE TABLE IF NOT EXISTS Posts ("
                                 "pid INTEGER PRIMARY KEY AUTOINCREMENT, "
                                 "title TEXT NOT NULL, "
                                 "content TEXT NOT NULL);";

    rc = sqlite3_exec(db, createPostsSQL, 0, 0, 0);

    if (rc != SQLITE_OK)
    {
        std::cerr << "Error creating table: " << sqlite3_errmsg(db) << std::endl;
        return int(Status::DATABASE_OPEN_ERROR);
    }

    const char *createCommentsSQL = "CREATE TABLE IF NOT EXISTS Comments ("
                                    "cid INTEGER PRIMARY KEY AUTOINCREMENT, "
                                    "pid INTEGER, "
                                    "content TEXT NOT NULL, "
                                    "FOREIGN KEY (pid) REFERENCES Posts(pid));";

    rc = sqlite3_exec(db, createCommentsSQL, 0, 0, 0);

    if (rc != SQLITE_OK)
    {
        std::cerr << "Error creating table: " << sqlite3_errmsg(db) << std::endl;
        return int(Status::DATABASE_OPEN_ERROR);
    }

    int server_fd = create_socket(nullptr, 8080); // Db server to be on port 8080

    while (true)
    {
        // Accept a connection
        std::cout << "Waiting to accept connection..." << std::endl;
        int client_socket = accept(server_fd, nullptr, nullptr);
        std::cout << "Connected." << std::endl;
        if (client_socket == -1)
        {
            std::cerr << "Error accepting connection" << std::endl;
            continue;
        }

        // Receive data from the client
        char buffer[BUFFER_SIZE];
        ssize_t bytesRead = recv(client_socket, buffer, sizeof(buffer), 0);

        // Turn char into str.
        std::string instruct(buffer, bytesRead);

        std::cout << "Received instruct " << instruct << std::endl;

        // Further split by space.
        // Find the position of the first space
        size_t space_pos = instruct.find(' ');

        // Split the string into two parts
        Command command = Command(stoi(instruct.substr(0, space_pos)));

        Status status;

        switch (command)
        {
        case Command::INSERT_POST:
        {
            // Further split to get title and content.
            std::string afterSpace = instruct.substr(space_pos + 1);
            size_t split_pos = afterSpace.find('|');
            std::string title = afterSpace.substr(0, split_pos);
            std::string content = afterSpace.substr(split_pos + 1);

            status = createPost(db, title, content);

            char ack[10];

            if (status == Status::INSERT_ERROR)
            {
                strcpy(ack, "FAIL");
                std::cout << "Sending FAIL status..." << std::endl;
                send(client_socket, ack, 10, 0);
            }
            else
            {
                strcpy(ack, "OK");
                std::cout << "Sending OK status..." << std::endl;
                send(client_socket, ack, 10, 0);
            }

            break;
        }
        case Command::RETRIEVE_POSTS:
            // In this case, we just want to get every post we can.
            status = getPosts(db, client_socket);

            // if (status == Status::INSERT_ERROR)
            // TODO: Handle failure case.
            break;
        default:
            // TODO: Handle failure case.
            break;
        }

        close(client_socket);
    }

    sqlite3_close(db);

    return 0;
}

/* Nice example:

const char *selectDataSQL = "SELECT * FROM Users;";

rc = sqlite3_exec(
    db, selectDataSQL, [](void *data, int argc, char **argv, char **colName) -> int
    {
    for (int i = 0; i < argc; i++) {
        std::cout << colName[i] << ": " << (argv[i] ? argv[i] : "NULL") << " | ";
    }
    std::cout << std::endl;

    return 0; },
    0, 0);

if (rc != SQLITE_OK)
{
    std::cerr << "Error querying data: " << sqlite3_errmsg(db) << std::endl;
    return rc;
}
*/
