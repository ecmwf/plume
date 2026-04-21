async function readYamlIntoEditor(fileInput, editorId) {
  const picked = fileInput.files && fileInput.files[0];
  if (!picked) {
    return;
  }
  const editor = document.getElementById(editorId);
  if (!editor) {
    return;
  }
  const content = await picked.text();
  editor.value = content;
}

function bindUploadPicker(button, callbacks) {
  const fileInput = document.getElementById(button.dataset.target);
  if (!fileInput) {
    return;
  }

  const pathInput = button.parentElement.querySelector("input[type='text']");

  button.addEventListener("click", () => {
    fileInput.click();
  });

  fileInput.addEventListener("change", async () => {
    const picked = fileInput.files && fileInput.files[0];
    if (!picked || !pathInput) {
      return;
    }

    pathInput.value = picked.name;

    if (fileInput.id === "plume-upload-file") {
      await readYamlIntoEditor(fileInput, "plume-yaml-editor");
      if (callbacks.onPlumeYamlUploaded) {
        await callbacks.onPlumeYamlUploaded(document.getElementById("plume-yaml-editor").value, picked.name);
      }
    }
    if (fileInput.id === "emulator-upload-file") {
      await readYamlIntoEditor(fileInput, "emulator-yaml-editor");
      if (callbacks.onEmulatorYamlUploaded) {
        await callbacks.onEmulatorYamlUploaded(document.getElementById("emulator-yaml-editor").value, picked.name);
      }
    }
  });

}

export function initUploadHandlers(uploadButtons, callbacks) {
  uploadButtons.forEach((button) => {
    bindUploadPicker(button, callbacks || {});
  });
}
