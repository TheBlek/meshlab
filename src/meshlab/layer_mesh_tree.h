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

} // namespace meshlab

#endif // MESHLAB_LAYER_MESH_TREE_H
