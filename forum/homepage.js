document.getElementById('createNewPost').addEventListener('click', function(event) {

    event.preventDefault();

    // Specify the target IP address and port
    const ipAddress = 'localhost';
    const port = '80';

    var title = document.getElementById('title').value;
    var message = document.getElementById('message').value;

    // Construct the URL with parameters
    const url = `http://${ipAddress}:${port}/insert_post?title=` + title + `&content=` + message;

    // Perform a simple GET request using the Fetch API
    fetch(url)
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! Status: ${response.status}`);
            }
            return response.text();
        })
        .catch(error => {
            console.error('Error sending message:', error);
        });
});