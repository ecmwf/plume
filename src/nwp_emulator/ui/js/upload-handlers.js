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

function bindUploadPicker(button) {
  const fileInput = document.getElementById(button.dataset.target);
  if (!fileInput) {
    return;
  }

  const pathInput = button.parentElement.querySelector("input[type='text']");
  const dirInput = button.dataset.dirTarget ? document.getElementById(button.dataset.dirTarget) : null;

  button.addEventListener("click", () => {
    if (dirInput) {
      const chooseFolder = window.confirm("OK: choose a folder. Cancel: choose .grib/.grib1/.grib2 files.");
      if (chooseFolder) {
        dirInput.click();
        return;
      }
    }
    fileInput.click();
  });

  fileInput.addEventListener("change", async () => {
    const picked = fileInput.files && fileInput.files[0];
    if (!picked || !pathInput) {
      return;
    }

    if (fileInput.id === "grib-upload-file") {
      pathInput.value = fileInput.files.length > 1
        ? `${fileInput.files.length} grib files selected`
        : picked.name;
      return;
    }

    pathInput.value = picked.name;

    if (fileInput.id === "plume-upload-file") {
      await readYamlIntoEditor(fileInput, "plume-yaml-editor");
    }
    if (fileInput.id === "emulator-upload-file") {
      await readYamlIntoEditor(fileInput, "emulator-yaml-editor");
    }
  });

  if (dirInput) {
    dirInput.addEventListener("change", () => {
      const picked = dirInput.files && dirInput.files[0];
      if (!picked || !pathInput) {
        return;
      }
      const rel = picked.webkitRelativePath || picked.name;
      const folderName = rel.split("/")[0];
      pathInput.value = `${folderName}/`;
    });
  }
}

export function initUploadHandlers(uploadButtons) {
  uploadButtons.forEach(bindUploadPicker);
}
