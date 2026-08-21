/****************************************************************************
* MeshLab
****************************************************************************/

#include "layer_mesh_tree.h"

#include <QFileInfo>

MeshTreeWidgetItem::MeshTreeWidgetItem(
	int meshid,
	const QString& label,
	const QString& tooltip,
	bool visible,
	bool modified) :
	QTreeWidgetItem(),
	_rendertoolbar(nullptr),
	_meshid(meshid)
{
	updateVisibilityIcon(visible);
	setText(1, QString::number(meshid));
	setDisplayLabel(label, modified);
	setToolTip(2, tooltip);
}

MeshTreeWidgetItem::~MeshTreeWidgetItem()
{
}

void MeshTreeWidgetItem::updateVisibilityIcon(bool isvisible)
{
	if (isvisible)
		setIcon(0, QIcon(":/images/layer_eye_open.png"));
	else
		setIcon(0, QIcon(":/images/layer_eye_close.png"));
}

void MeshTreeWidgetItem::setDisplayLabel(const QString& label, bool modified)
{
	setText(2, meshlab::meshLayerDisplayName(label, modified));
}

namespace meshlab {

QString meshLayerDisplayName(const QString& label, bool modified)
{
	QFileInfo inf(label);
	QString name = inf.completeBaseName();
	if (modified)
		name += " *";
	return name;
}

MeshTreeWidgetItem* appendMeshLayerRow(
	QTreeWidget* tree,
	int meshId,
	const QString& label,
	const QString& tooltip,
	bool visible,
	bool modified,
	QWidget* renderToolbar)
{
	if (tree == nullptr)
		return nullptr;

	if (findTopLevelMeshRow(tree, meshId) != nullptr)
		return nullptr;

	auto* item = new MeshTreeWidgetItem(meshId, label, tooltip, visible, modified);
	tree->addTopLevelItem(item);
	if (renderToolbar != nullptr)
		tree->setItemWidget(item, 3, renderToolbar);
	return item;
}

static void addNoteRow(QTreeWidgetItem* parent, const QString& key, const QString& value)
{
	auto* child = new QTreeWidgetItem(parent);
	child->setText(1, key);
	if (!value.isEmpty())
		child->setText(2, value);
}

void setMeshLayerBasicNotes(
	QTreeWidgetItem* parent,
	const QString& fileName,
	int vertexCount,
	int edgeCount,
	int faceCount)
{
	if (parent == nullptr)
		return;

	addNoteRow(parent, QString("File"), fileName);
	addNoteRow(parent, QString("Vertices"), QString::number(vertexCount));
	if (edgeCount > 0)
		addNoteRow(parent, QString("Edges"), QString::number(edgeCount));
	addNoteRow(parent, QString("Faces"), QString::number(faceCount));
}

void replaceMeshLayerBasicNotes(
	QTreeWidgetItem* parent,
	const QString& fileName,
	int vertexCount,
	int edgeCount,
	int faceCount)
{
	if (parent == nullptr)
		return;
	while (parent->childCount() > 0)
		delete parent->takeChild(0);
	setMeshLayerBasicNotes(parent, fileName, vertexCount, edgeCount, faceCount);
}

int topLevelMeshRowCount(const QTreeWidget* tree)
{
	if (tree == nullptr)
		return 0;
	int count = 0;
	for (int i = 0; i < tree->topLevelItemCount(); ++i) {
		if (dynamic_cast<const MeshTreeWidgetItem*>(tree->topLevelItem(i)) != nullptr)
			++count;
	}
	return count;
}

MeshTreeWidgetItem* findTopLevelMeshRow(QTreeWidget* tree, int meshId)
{
	if (tree == nullptr)
		return nullptr;
	for (int i = 0; i < tree->topLevelItemCount(); ++i) {
		auto* item = dynamic_cast<MeshTreeWidgetItem*>(tree->topLevelItem(i));
		if (item != nullptr && item->_meshid == meshId)
			return item;
	}
	return nullptr;
}

} // namespace meshlab
