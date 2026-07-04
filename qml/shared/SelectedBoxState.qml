import QtQml

QtObject {
    id: selectedBoxState

    property var editor: null
    property int revision: 0

    function value(roleName, fallback) {
        revision;
        if (!editor || editor.selectedIndex < 0)
            return fallback;

        const value = editor.boxRole(editor.selectedIndex, roleName);
        return value === undefined || value === null ? fallback : value;
    }

    function effectValue(effectName, propertyName, legacyRoleName, fallback) {
        const effects = value("boxEffects", null);
        if (effects !== null && effects !== undefined) {
            const effect = effects[effectName];
            if (effect !== null && effect !== undefined) {
                const groupedValue = effect[propertyName];
                if (groupedValue !== undefined && groupedValue !== null)
                    return groupedValue;
            }
        }

        return value(legacyRoleName, fallback);
    }

    function modelIndexRow(modelIndex) {
        if (!modelIndex)
            return -1;
        if (typeof modelIndex.row === "function")
            return modelIndex.row();
        if (modelIndex.row !== undefined)
            return modelIndex.row;
        return -1;
    }

    function rolesAffectSelectedBoxState(roles) {
        if (!roles || roles.length === 0)
            return true;
        if (!editor || typeof editor.boxRolesAffectSelectedBoxState !== "function")
            return true;
        return editor.boxRolesAffectSelectedBoxState(roles);
    }

    function invalidateForDataChanged(topLeft, bottomRight, roles) {
        if (!editor) {
            revision += 1;
            return;
        }

        const selected = editor.selectedIndex;
        const first = modelIndexRow(topLeft);
        const last = modelIndexRow(bottomRight);
        if (first >= 0 && last >= 0 && (selected < first || selected > last))
            return;

        if (!rolesAffectSelectedBoxState(roles))
            return;

        revision += 1;
    }

    property Connections boxesModelConnections: Connections {
        target: selectedBoxState.editor ? selectedBoxState.editor.boxesModel : null
        function onDataChanged(topLeft, bottomRight, roles) {
            selectedBoxState.invalidateForDataChanged(topLeft, bottomRight, roles);
        }
        function onModelReset() { selectedBoxState.revision += 1; }
        function onRowsInserted() { selectedBoxState.revision += 1; }
        function onRowsRemoved() { selectedBoxState.revision += 1; }
    }
}
