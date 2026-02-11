document.addEventListener('DOMContentLoaded', () => {
    const fileList = document.getElementById('file-list');
    const addFileBtn = document.getElementById('add-file-btn');
    const form = document.getElementById('upload-form');
    const statusDiv = document.getElementById('status');
    const textInput = document.getElementById('text-input');

    // Function to add a new file input field
    function addFileInput() {
        const wrapper = document.createElement('div');
        wrapper.className = 'file-input-wrapper';
        wrapper.style.marginBottom = '10px';
        wrapper.style.display = 'flex';
        wrapper.style.alignItems = 'center';
        wrapper.style.gap = '10px';

        const input = document.createElement('input');
        input.type = 'file';
        input.name = 'files';
        input.style.color = 'white';

        const removeBtn = document.createElement('button');
        removeBtn.type = 'button';
        removeBtn.textContent = '削除';
        removeBtn.style.cursor = 'pointer';
        removeBtn.style.padding = '4px 8px';
        removeBtn.style.backgroundColor = 'rgba(255, 100, 100, 0.2)';
        removeBtn.style.border = '1px solid rgba(255, 100, 100, 0.4)';
        removeBtn.style.color = 'white';
        removeBtn.style.borderRadius = '4px';
        
        removeBtn.onclick = () => {
            wrapper.remove();
        };

        wrapper.appendChild(input);
        wrapper.appendChild(removeBtn);
        fileList.appendChild(wrapper);
    }

    // Add initial file input
    addFileInput();

    // Event listener for adding more file inputs
    addFileBtn.addEventListener('click', addFileInput);

    // Handle form submission
    form.addEventListener('submit', async (e) => {
        e.preventDefault();
        statusDiv.textContent = 'Uploading...';
        statusDiv.style.color = 'var(--accent)';

        const formData = new FormData();
        
        // Append all selected files from file inputs
        const fileInputs = fileList.querySelectorAll('input[type="file"]');
        let fileCount = 0;
        fileInputs.forEach(input => {
            if (input.files.length > 0) {
                for (let i = 0; i < input.files.length; i++) {
                    formData.append('files', input.files[i]);
                    fileCount++;
                }
            }
        });

        // Append text input as a file if it has content
        const textContent = textInput.value;
        if (textContent.trim() !== '') {
            const textFile = new File([textContent], "input.txt", { type: "text/plain" });
            formData.append('files', textFile);
            fileCount++;
        }

        if (fileCount === 0) {
            statusDiv.textContent = 'No files to upload.';
            statusDiv.style.color = 'orange';
            return;
        }

        try {
            const response = await fetch('/upload', {
                method: 'POST',
                body: formData
            });

            if (response.ok) {
                const resultText = await response.text();
                statusDiv.textContent = `Upload successful! (${response.status}) ${resultText}`;
                statusDiv.style.color = '#4ade80'; // Green
                // Optional: Clear form
                // form.reset(); 
                // fileList.innerHTML = ''; addFileInput();
            } else {
                statusDiv.textContent = `Upload failed: ${response.status} ${response.statusText}`;
                statusDiv.style.color = '#f87171'; // Red
            }
        } catch (error) {
            statusDiv.textContent = `Error: ${error.message}`;
            statusDiv.style.color = '#f87171';
        }
    });
});
