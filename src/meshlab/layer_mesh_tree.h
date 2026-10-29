/****************************************************************************
* MeshLab
*
* Helpers for the layer-dialog mesh tree so rows can be appended without
* rebuilding the whole QTreeWidget. Kept free of MeshModel/MainWindow so
* the basics can be tested with Qt Test alone.
****************************************************************************/

#ifndef MESHLAB_LAYER_MESH_TREE_H
#define MESHLAB_LAYER_MESH_TREE_H

#include <QIcon>
#include <QMap>
#include <QPair>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QWidget>

class MLRenderingToolbar;

class MeshTreeWidgetItem : public QTreeWidgetItem
{
public:
	MeshTreeWidgetItem(
		int meshid,
		const QString& label,
		const QString& tooltip,
		bool visible,
		bool modified);
	~MeshTreeWidgetItem();

	void updateVisibilityIcon(bool isvisible);
	void setDisplayLabel(const QString& label, bool modified);

	MLRenderingToolbar* _rendertoolbar;
	int _meshid;
};

namespace meshlab {

QString meshLayerDisplayName(const QString& label, bool modified);

MeshTreeWidgetItem* appendMeshLayerRow(
	QTreeWidget* tree,
	int meshId,
	const QString& label,
	const QString& tooltip,
	bool visible,
	bool modified,
	QWidget* renderToolbar);

void setMeshLayerBasicNotes(
	QTreeWidgetItem* parent,
	const QString& fileName,
	int vertexCount,
	int edgeCount,
	int faceCount);

void replaceMeshLayerBasicNotes(
	QTreeWidgetItem* parent,
	const QString& fileName,
	int vertexCount,
	int edgeCount,
	int faceCount);

int topLevelMeshRowCount(const QTreeWidget* tree);
MeshTreeWidgetItem* findTopLevelMeshRow(QTreeWidget* tree, int meshId);

// Tag stored by the layer dialog for a note row. Non-numeric column 2
// (file names) uses 0, matching QString::toInt's failure value.
int noteExpansionTag(const QString& column2Text);

// Stable key for a note row so expansion survives a count change
// (Vertices "10" -> "10000") and an Edges row appearing.
int noteExpansionLabelKey(const QString& column1Text, const QString& column2Text);

void rememberNoteExpansion(
	QTreeWidgetItem* parent,
	int meshId,
	QMap<QPair<int, int>, bool>& expansion);

void restoreNoteExpansion(
	QTreeWidgetItem* parent,
	int meshId,
	const QMap<QPair<int, int>, bool>& expansion);

} // namespace meshlab

#endif // MESHLAB_LAYER_MESH_TREE_H
