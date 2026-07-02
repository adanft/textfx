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

    property Connections boxesModelConnections: Connections {
        target: selectedBoxState.editor ? selectedBoxState.editor.boxesModel : null
        function onDataChanged() { selectedBoxState.revision += 1; }
        function onModelReset() { selectedBoxState.revision += 1; }
        function onRowsInserted() { selectedBoxState.revision += 1; }
        function onRowsRemoved() { selectedBoxState.revision += 1; }
    }
}
