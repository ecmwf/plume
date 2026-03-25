export async function readYamlIntoEditor(fileInput, editorId) {
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
  const folderOnly = button.dataset.folderOnly === "true";
  const isDirectoryInput = fileInput.hasAttribute("webkitdirectory") || fileInput.hasAttribute("directory");

  button.addEventListener("click", () => {
    if (folderOnly || isDirectoryInput) {
      fileInput.click();
      return;
    }
    fileInput.click();
  });

  fileInput.addEventListener("change", async () => {
    const allFiles = Array.from(fileInput.files || []);
    const picked = allFiles[0];
    if (!picked || !pathInput) {
      return;
    }

    if (folderOnly || isDirectoryInput) {
      const relativePaths = allFiles.map((item) => item.webkitRelativePath || item.name);
      const folderName = (picked.webkitRelativePath || picked.name).split("/")[0] || "selected-folder";
      const topLevelEntries = relativePaths
        .filter((entry) => entry.split("/").length === 2)
        .map((entry) => entry.split("/")[1]);

      pathInput.value = `${folderName}/`;
      if (callbacks.onGribFolderSelected) {
        await callbacks.onGribFolderSelected(folderName, topLevelEntries);
      }
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
