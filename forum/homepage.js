const createNewPost = document.getElementById('createNewPost');

function handlePostEvent(event) {
    event.stopPropagation();
    event.preventDefault();

    // Specify the target IP address and port
    const ipAddress = 'localhost';
    const port = '80';

    var title = document.getElementById('title').value;
    var message = document.getElementById('message').value;

    // Construct the URL with parameters
    const url = `http://${ipAddress}:${port}/insert_post?title=${title}&content=${message}`;

    // Perform a simple GET request using the Fetch API
    fetch(url)
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! Status: ${response.status}`);
            }
            return response.text();
        })
        .then(data => {
            // Reload the page after receiving the response
            window.location.reload();
        })
        .catch(error => {
            console.error('Error sending message:', error);
        });
}

// Listen for both mousedown and touchstart events
createNewPost.addEventListener('click', handlePostEvent, false);
