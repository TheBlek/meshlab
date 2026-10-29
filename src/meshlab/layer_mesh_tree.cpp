/****************************************************************************
* MeshLab
****************************************************************************/

#include "layer_mesh_tree.h"

#include <QFileInfo>
#include <QHash>

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

int noteExpansionTag(const QString& column2Text)
{
	bool ok = false;
	const int tag = column2Text.toInt(&ok);
	if (!ok)
		return 0;
	return tag;
}

int noteExpansionLabelKey(const QString& column1Text, const QString& column2Text)
{
	const QString role = column1Text.isEmpty() ? column2Text : column1Text;
	if (role == QLatin1String("File"))
		return 1000000001;
	if (role == QLatin1String("Vertices"))
		return 1000000002;
	if (role == QLatin1String("Edges"))
		return 1000000003;
	if (role == QLatin1String("Faces"))
		return 1000000004;
	if (role == QLatin1String("Textures:"))
		return 1000000005;
	if (role == QLatin1String("Custom Attrs:"))
		return 1000000006;
	// High bit keeps hashed custom rows away from counts and from the mesh
	// row's tag (-1).
	return int(qHash(role) & 0x0fffffff) | 0x50000000;
}

void rememberNoteExpansion(
	QTreeWidgetItem* parent,
	int meshId,
	QMap<QPair<int, int>, bool>& expansion)
{
	if (parent == nullptr)
		return;
	for (int i = 0; i < parent->childCount(); ++i) {
		QTreeWidgetItem* child = parent->child(i);
		const bool expanded = child->isExpanded();
		expansion.insert(qMakePair(meshId, noteExpansionTag(child->text(2))), expanded);
		expansion.insert(
			qMakePair(meshId, noteExpansionLabelKey(child->text(1), child->text(2))),
			expanded);
	}
}

void restoreNoteExpansion(
	QTreeWidgetItem* parent,
	int meshId,
	const QMap<QPair<int, int>, bool>& expansion)
{
	if (parent == nullptr)
		return;
	for (int i = 0; i < parent->childCount(); ++i) {
		QTreeWidgetItem* child = parent->child(i);
		const QPair<int, int> labelKey = qMakePair(
			meshId, noteExpansionLabelKey(child->text(1), child->text(2)));
		const QPair<int, int> tagKey = qMakePair(meshId, noteExpansionTag(child->text(2)));
		if (expansion.contains(labelKey))
			child->setExpanded(expansion.value(labelKey));
		else if (expansion.contains(tagKey))
			child->setExpanded(expansion.value(tagKey));
	}
}

} // namespace meshlab
